/*Min Vehicle Time is a "Time Series" Plugin*/
select
  time,
  (msg -> 'phases' -> 0 -> 'vehTimeMin') :: integer as lane1,
  (msg -> 'phases' -> 1 -> 'vehTimeMin') :: integer as lane2,
  (msg -> 'phases' -> 2 -> 'vehTimeMin') :: integer as lane3,
  (msg -> 'phases' -> 3 -> 'vehTimeMin') :: integer as lane4,
  (msg -> 'phases' -> 4 -> 'vehTimeMin') :: integer as lane5,
  (msg -> 'phases' -> 5 -> 'vehTimeMin') :: integer as lane6,
  (msg -> 'phases' -> 6 -> 'vehTimeMin') :: integer as lane7,
  (msg -> 'phases' -> 7 -> 'vehTimeMin') :: integer as lane8
from
  natsql."light-status"
WHERE
  $__timeFilter(time)
ORDER BY
  time