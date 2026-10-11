package com.harness;

import com.fasterxml.jackson.databind.ObjectMapper;
import org.springframework.beans.factory.annotation.Value;
import org.springframework.data.domain.PageRequest;
import org.springframework.http.*;
import org.springframework.web.bind.annotation.*;
import java.util.*;

@RestController
@RequestMapping("/api")
public class ApiController {
    private final HarnessService service;
    private final ReadingRepo readings;
    private final EventRepo events;
    private final ObjectMapper mapper;
    @Value("${harness.api-key}") private String apiKey;

    public ApiController(HarnessService s, ReadingRepo r, EventRepo e, ObjectMapper m) {
        service = s; readings = r; events = e; mapper = m;
    }

    /** The ESP32 posts here. */
    @PostMapping("/ingest")
    public ResponseEntity<?> ingest(@RequestHeader(value = "X-API-Key", required = false) String key,
                                    @RequestBody Map<String, Object> body) {
        if (!apiKey.equals(key)) return ResponseEntity.status(401).body(Map.of("error", "bad api key"));
        service.record(body);
        return ResponseEntity.ok(Map.of("ok", true));
    }

    @GetMapping("/latest")
    @SuppressWarnings("unchecked")
    public Map<String, Object> latest() {
        Reading r = readings.findFirstByOrderByIdDesc();
        if (r == null) return Map.of();
        Map<String, Object> m = mapper.convertValue(r, Map.class);
        m.put("age", System.currentTimeMillis() / 1000.0 - r.ts);
        return m;
    }

    @GetMapping("/history")
    public List<Reading> history(@RequestParam(defaultValue = "10") double minutes) {
        List<Reading> all = readings.findByTsGreaterThanOrderByTs(System.currentTimeMillis() / 1000.0 - minutes * 60);
        int step = Math.max(1, all.size() / 300);
        List<Reading> out = new ArrayList<>();
        for (int i = 0; i < all.size(); i += step) out.add(all.get(i));
        return out;
    }

    @GetMapping("/events")
    public List<Event> events(@RequestParam(defaultValue = "50") int limit) {
        return events.findAllByOrderByIdDesc(PageRequest.of(0, limit));
    }

    @GetMapping("/export.csv")
    public ResponseEntity<String> export() {
        StringBuilder sb = new StringBuilder("id,ts,hr,temp,fsr,accel,fall,pressure,alarm,sms,gpsFix,lat,lng\n");
        for (Reading r : readings.findAll())
            sb.append(r.id).append(',').append(r.ts).append(',').append(r.hr == null ? "" : r.hr).append(',')
              .append(r.temp == null ? "" : r.temp).append(',').append(r.fsr).append(',').append(r.accel).append(',')
              .append(r.fall).append(',').append(r.pressure).append(',').append(r.alarm).append(',').append(r.sms)
              .append(',').append(r.gpsFix).append(',').append(r.lat == null ? "" : r.lat).append(',')
              .append(r.lng == null ? "" : r.lng).append('\n');
        return ResponseEntity.ok()
            .header(HttpHeaders.CONTENT_DISPOSITION, "attachment; filename=harness_readings.csv")
            .contentType(MediaType.parseMediaType("text/csv")).body(sb.toString());
    }
}
