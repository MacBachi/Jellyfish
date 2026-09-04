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

extern Canvas canvas; // the frame buffer core 1 draws into, see JellyFloatOS.cpp

namespace
{
    constexpr size_t STATE_JSON_MAX = 3072; // 16 roster entries and the header fit with room to spare
    constexpr size_t CMD_MAX = 64;          // same as a network line
    constexpr int POST_SLOTS = 4;           // concurrent POSTs; the page sends one at a time

    // One command being received. httpd hands us the body in pieces.
    struct PostState
    {
        void* connection = nullptr;
        bool findmy = false; // POST /api/findmy rather than /api/cmd
        char line[CMD_MAX];
        size_t len = 0;
    };
    PostState posts[POST_SLOTS];

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
    const bool findmy = strcmp(uri, "/api/findmy") == 0;
    if ((!findmy && strcmp(uri, "/api/cmd") != 0) || content_len < 0 || content_len >= (int)CMD_MAX)
    {
        snprintf(response_uri, response_uri_len, "/400.txt");
        return ERR_VAL;
    }
    PostState* p = post_for(nullptr);
    if (p == nullptr)
    {
        snprintf(response_uri, response_uri_len, "/400.txt");
        return ERR_MEM;
    }
    p->connection = connection;
    p->findmy = findmy;
    p->len = 0;
    return ERR_OK;
}

extern "C" err_t httpd_post_receive_data(void* connection, pbuf* q)
{
    PostState* p = post_for(connection);
    if (p != nullptr)
    {
        const size_t room = CMD_MAX - 1 - p->len;
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
    p->line[p->len] = 0;
    for (char* c = p->line; *c; ++c)
        if (*c == '\r' || *c == '\n')
        {
            *c = 0;
            break;
        }
    p->connection = nullptr;

    if (p->line[0] == 0)
    {
        snprintf(response_uri, response_uri_len, "/400.txt");
        return;
    }
    if (p->findmy)
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
