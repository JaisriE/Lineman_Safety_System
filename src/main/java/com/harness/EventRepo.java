package com.harness;

import org.springframework.data.domain.Pageable;
import org.springframework.data.jpa.repository.JpaRepository;
import java.util.List;

public interface EventRepo extends JpaRepository<Event, Long> {
    List<Event> findAllByOrderByIdDesc(Pageable p);
}
