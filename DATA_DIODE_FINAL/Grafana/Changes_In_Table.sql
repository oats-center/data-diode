SELECT 
CASE WHEN 
(msg -> 'phases' -> 6 -> 'vehTimeMin') :: integer = 
(msg -> 'phases' -> 6 -> 'vehTimeMax') :: integer 
THEN 'changes in '|| (msg -> 'phases' -> 6 -> 'vehTimeMin') :: integer ELSE NULL
END  FROM "light-status" ORDER BY time DESC LIMIT 1