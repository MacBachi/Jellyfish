import Foundation

/// Keeps the offset between the phone's clock and the AP's clock, exactly like a station
/// in the firmware does: first value taken as is, jumps over 50 ms snap, smaller ones slew.
struct TimeSync {
    private(set) var offsetUs: Int64 = 0
    private(set) var hasOffset = false

    /// Microseconds of the local monotonic clock.
    static func nowUs() -> Int64 { Int64(DispatchTime.now().uptimeNanoseconds / 1000) }

    mutating func update(apTimeUs: Int64, receivedAtUs rx: Int64) {
        let fresh = apTimeUs - rx
        if !hasOffset {
            offsetUs = fresh
            hasOffset = true
        } else {
            let delta = fresh - offsetUs
            if abs(delta) > 50_000 { offsetUs = fresh } else { offsetUs += delta / 4 }
        }
    }

    mutating func reset() { offsetUs = 0; hasOffset = false }

    /// The AP's clock, in microseconds, as best we know it.
    func masterNowUs() -> Int64 { TimeSync.nowUs() + offsetUs }
}
