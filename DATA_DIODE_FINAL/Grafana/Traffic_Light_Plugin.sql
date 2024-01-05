SELECT time, 
  CASE
    WHEN ('"RED"' = (msg-> 'phases' -> 6 -> 'color') :: text ) THEN 1 
    WHEN ('"GREEN"' = (msg-> 'phases' -> 6 -> 'color') :: text ) THEN 2 
    WHEN ('"YELLOW"' = (msg-> 'phases' -> 6 -> 'color') :: text ) THEN 3 
    ELSE 0 END as LANE7
FROM "light-status" ORDER BY
    time DESC
  LIMIT 1