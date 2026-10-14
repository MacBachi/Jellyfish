#include "jell_web.hpp"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "pico/cyw43_arch.h"
#include "lwip/apps/httpd.h"
#include "lwip/apps/fs.h"
#include "lwip/pbuf.h"

#include "jell_canvas.hpp"
#include "jell_net.hpp"
#include "jell_findmy.hpp"
#include "jell_ota.hpp"

extern Canvas canvas; // the frame buffer core 1 draws into, see JellyFloatOS.cpp

namespace
{
    constexpr size_t STATE_JSON_MAX = 3072; // 16 roster entries and the header fit with room to spare
    constexpr size_t CMD_MAX = 64;          // commands are short; network lines may be longer (NET_LINE_MAX)
    constexpr size_t BEGIN_MAX = 160;       // "<size> <128 hex digits>" for /api/ota/begin
    constexpr int POST_SLOTS = 4;           // concurrent POSTs; the page sends one at a time

    enum class PostKind : uint8_t { Cmd, FindMy, OtaBegin, OtaChunk, OtaFinish };

    // One command being received. httpd hands us the body in pieces.
    struct PostState
    {
        void* connection = nullptr;
        PostKind kind = PostKind::Cmd;
        char line[BEGIN_MAX];
        size_t len = 0;
        size_t max = CMD_MAX;
    };
    PostState posts[POST_SLOTS];

    // An update chunk: 4-byte offset, then the data. One at a time; the app waits for each.
    uint8_t ota_buf[4 + OTA::CHUNK];
    size_t ota_len = 0;
    void* ota_connection = nullptr;

    PostState* post_for(void* connection)
    {
        for (PostState& p : posts)
            if (p.connection == connection)
                return &p;
        return nullptr;
    }

    // A generated file: the buffer is malloc'd here and freed in fs_close_custom.
    bool serve(fs_file* file, char* buf, size_t len)
    {
        file->data = buf;
        file->len = (int)len;
        file->index = (int)len;
        // No headers of our own: httpd builds them from the extension (.json, .bin, .txt),
        // and HEADER_PERSISTENT makes it add Content-Length so the connection can stay open.
        file->flags = FS_FILE_FLAGS_HEADER_PERSISTENT;
        return true;
    }
}

// ---------------------------------------------------------------------- custom files
// Called by httpd (lwIP context, core 0) for every request before the files in flash.

extern "C" int fs_open_custom(fs_file* file, const char* name)
{
    if (strcmp(name, "/api/state.json") == 0)
    {
        char* buf = (char*)malloc(STATE_JSON_MAX);
        if (buf == nullptr)
            return 0;
        const size_t n = Net::write_status_json(buf, STATE_JSON_MAX);
        return serve(file, buf, n);
    }

    if (strcmp(name, "/api/frame.bin") == 0)
    {
        char* buf = (char*)malloc(Canvas::FRAME_BYTES);
        if (buf == nullptr)
            return 0;
        canvas.copy_frame((uint8_t*)buf);
        return serve(file, buf, Canvas::FRAME_BYTES);
    }

    if (strcmp(name, "/api/ota.json") == 0)
    {
        char* buf = (char*)malloc(320);
        if (buf == nullptr) return 0;
        const size_t n = OTA::write_json(buf, 320);
        return serve(file, buf, n);
    }

    if (strcmp(name, "/api/findmy.json") == 0)
    {
        char* buf = (char*)malloc(256);
        if (buf == nullptr) return 0;
        const size_t n = FindMy::write_result_json(buf, 256);
        serve(file, buf, n);
        return 1;
    }
    if (strcmp(name, "/api/ok.json") == 0)
    {
        static const char ok[] = "{\"ok\":true}";
        char* buf = (char*)malloc(sizeof ok);
        if (buf == nullptr)
            return 0;
        memcpy(buf, ok, sizeof ok);
        return serve(file, buf, sizeof ok - 1);
    }

    if (strcmp(name, "/400.txt") == 0)
    {
        static const char bad[] = "bad request";
        char* buf = (char*)malloc(sizeof bad);
        if (buf == nullptr)
            return 0;
        memcpy(buf, bad, sizeof bad);
        return serve(file, buf, sizeof bad - 1);
    }

    return 0; // not ours: httpd looks in the flash file system next
}

extern "C" void fs_close_custom(fs_file* file)
{
    free((void*)file->data);
    file->data = nullptr;
}

extern "C" int fs_read_custom(fs_file*, char*, int)
{
    return FS_READ_EOF; // everything is in file->data already
}

// ---------------------------------------------------------------------- POST /api/cmd

extern "C" err_t httpd_post_begin(void* connection, const char* uri, const char*, u16_t, int content_len,
                                  char* response_uri, u16_t response_uri_len, u8_t*)
{
    PostKind kind;
    size_t max = CMD_MAX;
    if (strcmp(uri, "/api/cmd") == 0) kind = PostKind::Cmd;
    else if (strcmp(uri, "/api/findmy") == 0) kind = PostKind::FindMy;
    else if (strcmp(uri, "/api/ota/begin") == 0) { kind = PostKind::OtaBegin; max = BEGIN_MAX; }
    else if (strcmp(uri, "/api/ota/chunk") == 0) { kind = PostKind::OtaChunk; max = sizeof ota_buf + 1; }
    else if (strcmp(uri, "/api/ota/finish") == 0) kind = PostKind::OtaFinish;
    else
    {
        snprintf(response_uri, response_uri_len, "/400.txt");
        return ERR_VAL;
    }
    if (content_len < 0 || content_len >= (int)max)
    {
        snprintf(response_uri, response_uri_len, "/400.txt");
        return ERR_VAL;
    }
    if (kind == PostKind::OtaChunk)
    {
        if (ota_connection != nullptr)
        {
            snprintf(response_uri, response_uri_len, "/api/ota.json");
            return ERR_INPROGRESS;
        }
        ota_connection = connection;
        ota_len = 0;
    }
    PostState* p = post_for(nullptr);
    if (p == nullptr)
    {
        if (kind == PostKind::OtaChunk) ota_connection = nullptr;
        snprintf(response_uri, response_uri_len, "/400.txt");
        return ERR_MEM;
    }
    p->connection = connection;
    p->kind = kind;
    p->len = 0;
    p->max = max;
    return ERR_OK;
}

extern "C" err_t httpd_post_receive_data(void* connection, pbuf* q)
{
    PostState* p = post_for(connection);
    if (p != nullptr && p->kind == PostKind::OtaChunk)
    {
        const size_t room = sizeof ota_buf - ota_len;
        const u16_t n = pbuf_copy_partial(q, ota_buf + ota_len, (u16_t)(room < q->tot_len ? room : q->tot_len), 0);
        ota_len += n;
    }
    else if (p != nullptr)
    {
        const size_t room = p->max - 1 - p->len;
        const u16_t n = pbuf_copy_partial(q, p->line + p->len, (u16_t)(room < q->tot_len ? room : q->tot_len), 0);
        p->len += n;
    }
    pbuf_free(q);
    return ERR_OK;
}

extern "C" void httpd_post_finished(void* connection, char* response_uri, u16_t response_uri_len)
{
    PostState* p = post_for(connection);
    if (p == nullptr)
    {
        snprintf(response_uri, response_uri_len, "/400.txt");
        return;
    }
    p->connection = nullptr;

    // Over-the-air update: every answer is the update status, refused or not.
    if (p->kind == PostKind::OtaChunk)
    {
        ota_connection = nullptr;
        if (ota_len > 4)
        {
            uint32_t offset;
            memcpy(&offset, ota_buf, 4); // little-endian, like the chip
            OTA::chunk(offset, ota_buf + 4, (uint32_t)(ota_len - 4));
        }
        snprintf(response_uri, response_uri_len, "/api/ota.json");
        return;
    }
    if (p->kind == PostKind::OtaFinish)
    {
        OTA::finish();
        snprintf(response_uri, response_uri_len, "/api/ota.json");
        return;
    }

    p->line[p->len] = 0;
    for (char* c = p->line; *c; ++c)
        if (*c == '\r' || *c == '\n')
        {
            *c = 0;
            break;
        }
    if (p->kind == PostKind::OtaBegin)
    {
        OTA::begin(p->line);
        snprintf(response_uri, response_uri_len, "/api/ota.json");
        return;
    }

    if (p->line[0] == 0)
    {
        snprintf(response_uri, response_uri_len, "/400.txt");
        return;
    }
    if (p->kind == PostKind::FindMy)
    {
        FindMy::provision(p->line); // validated here, written in the next poll()
        snprintf(response_uri, response_uri_len, "/api/findmy.json");
        return;
    }
    Net::submit_line(p->line); // handled in the next poll(), on the core-0 main loop
    snprintf(response_uri, response_uri_len, "/api/ok.json");
}

// ---------------------------------------------------------------------- init

void Web::init()
{
    cyw43_arch_lwip_begin();
    httpd_init();
    cyw43_arch_lwip_end();
}
