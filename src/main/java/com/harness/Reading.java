package com.harness;

import jakarta.persistence.*;

@Entity
@Table(name = "readings", indexes = @Index(name = "i_ts", columnList = "ts"))
public class Reading {
    @Id @GeneratedValue(strategy = GenerationType.IDENTITY) public Long id;
    public double ts;            // unix seconds
    public Double hr;
    public Double temp;
    public int fsr;
    public double accel;
    public boolean fall, pressure, alarm, sms, gpsFix;
    public Double lat, lng;
}
