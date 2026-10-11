package com.harness;

import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;

public interface ReadingRepo extends JpaRepository<Reading, Long> {
    Reading findFirstByOrderByIdDesc();
    List<Reading> findByTsGreaterThanOrderByTs(double ts);
}
