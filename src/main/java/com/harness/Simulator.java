package com.harness;

import org.springframework.boot.autoconfigure.condition.ConditionalOnProperty;
import org.springframework.scheduling.annotation.Scheduled;
import org.springframework.stereotype.Component;
import java.util.*;

/** Acts like the ESP32 when harness.demo=true: one reading per second, a fall every 60 s. */
@Component
@ConditionalOnProperty(name = "harness.demo", havingValue = "true")
public class Simulator {
    private final HarnessService service;
    private final Random rnd = new Random();
    private int t = 0;

    public Simulator(HarnessService s) { service = s; }

    @Scheduled(fixedRate = 1000)
    public void tick() {
        t++;
        int c = t % 60;
        Map<String, Object> d = new HashMap<>();
        d.put("hr", 74 + Math.sin(t / 6.0) * 5 + rnd.nextDouble() * 2);
        d.put("temp", 36.5 + Math.sin(t / 20.0) * 0.2);
        d.put("fsr", (int) (1200 + rnd.nextDouble() * 150));
        d.put("accel", 1 + rnd.nextDouble() * 0.05);
        d.put("gpsFix", t > 4);
        d.put("lat", 13.0827 + t * 1e-6);
        d.put("lng", 80.2707);
        if (c >= 35 && c < 40) { d.put("fall", true); d.put("alarm", true); d.put("accel", 2.9); }
        else if (c >= 40 && c < 50) { d.put("fall", true); d.put("sms", true); d.put("accel", 1.0); }
        service.record(d);
    }
}
