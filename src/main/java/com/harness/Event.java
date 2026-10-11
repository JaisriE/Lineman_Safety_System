package com.harness;

import jakarta.persistence.*;

@Entity
@Table(name = "events")
public class Event {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY) public Long id;
    public double ts;
    @Column(length = 200) public String message;
    public boolean bad;
    public Event() {}
    public Event(String message, boolean bad) {
        this.ts = System.currentTimeMillis() / 1000.0; this.message = message; this.bad = bad;
    }
}
