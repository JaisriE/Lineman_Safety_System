package com.harness;

import org.springframework.boot.SpringApplication;
import org.springframework.boot.autoconfigure.SpringBootApplication;
import org.springframework.scheduling.annotation.EnableScheduling;

@SpringBootApplication
@EnableScheduling
public class HarnessApplication {
    public static void main(String[] args) { SpringApplication.run(HarnessApplication.class, args); }
}
