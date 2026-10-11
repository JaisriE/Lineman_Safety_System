package com.harness;

import org.springframework.stereotype.Service;
import java.util.Map;

/** Stores each reading and logs an event whenever a state changes. */
@Service
public class HarnessService {
    private final ReadingRepo readings;
    private final EventRepo events;
    private boolean pFall, pPressure, pAlarm, pSms, pGps, first = true;

    public HarnessService(ReadingRepo r, EventRepo e) { readings = r; events = e; }

    private static Double num(Object o) { return o instanceof Number n ? n.doubleValue() : null; }
    private static boolean bool(Object o) { return o instanceof Boolean b && b; }

    public synchronized void record(Map<String, Object> d) {
        Reading r = new Reading();
        r.ts = System.currentTimeMillis() / 1000.0;
        r.hr = num(d.get("hr"));
        r.temp = num(d.get("temp"));
        r.fsr = num(d.get("fsr")) == null ? 0 : num(d.get("fsr")).intValue();
        r.accel = num(d.get("accel")) == null ? 1.0 : num(d.get("accel"));
        r.fall = bool(d.get("fall"));
        r.pressure = bool(d.get("pressure"));
        r.alarm = bool(d.get("alarm"));
        r.sms = bool(d.get("sms"));
        r.gpsFix = bool(d.get("gpsFix"));
        r.lat = num(d.get("lat"));
        r.lng = num(d.get("lng"));
        readings.save(r);

        if (r.fall && !pFall) events.save(new Event("Possible fall detected", true));
        if (r.pressure && !pPressure) events.save(new Event("Abnormal pressure (FSR " + r.fsr + ")", true));
        if (r.alarm && !pAlarm) events.save(new Event("Alarm started, 5 s cancel window", true));
        if (r.sms && !pSms) events.save(new Event("Emergency SMS sent", true));
        if (pSms && !r.sms) events.save(new Event("Alert acknowledged or cancelled", false));
        if (!first && r.gpsFix && !pGps) events.save(new Event("GPS fix acquired", false));
        if (pGps && !r.gpsFix) events.save(new Event("GPS fix lost", false));

        pFall = r.fall; pPressure = r.pressure; pAlarm = r.alarm; pSms = r.sms; pGps = r.gpsFix; first = false;
    }
}
