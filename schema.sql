-- OPTIONAL: tables are created automatically by Spring (ddl-auto=update).
-- Run this only if you want to create the database/tables yourself.
CREATE DATABASE IF NOT EXISTS harness;
USE harness;
CREATE TABLE IF NOT EXISTS readings (
  id BIGINT AUTO_INCREMENT PRIMARY KEY, ts DOUBLE NOT NULL, hr DOUBLE, temp DOUBLE,
  fsr INT, accel DOUBLE, fall BIT, pressure BIT, alarm BIT, sms BIT, gps_fix BIT,
  lat DOUBLE, lng DOUBLE, INDEX i_ts (ts));
CREATE TABLE IF NOT EXISTS events (
  id BIGINT AUTO_INCREMENT PRIMARY KEY, ts DOUBLE NOT NULL, message VARCHAR(200), bad BIT);
