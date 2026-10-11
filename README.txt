SAFETY HARNESS DASHBOARD - JAVA (Spring Boot) + MySQL

NEEDS: Java 17+, Maven, MySQL running locally.

1. Edit src/main/resources/application.properties:
     spring.datasource.username / password  -> your MySQL login
     harness.api-key                        -> your secret (same in the .ino file)
     harness.demo=true                      -> simulator (no ESP32). false for real device.
2. Run:   mvn spring-boot:run
3. Open:  http://localhost:5000
   Tables (readings, events) in database "harness" are created automatically.
   Check data in MySQL:  SELECT * FROM harness.readings ORDER BY id DESC LIMIT 10;

REAL ESP32: set harness.demo=false, then put your PC IP in esp32_backend_client.ino
(SERVER_URL = http://YOUR_PC_IP:5000/api/ingest), same API key, flash the board.
Test without the board:
  curl -X POST http://localhost:5000/api/ingest -H "Content-Type: application/json" -H "X-API-Key: change-me" -d '{"hr":78,"temp":36.6,"fsr":1200,"accel":1.0,"fall":false,"pressure":false,"alarm":false,"sms":false,"gpsFix":false}'
