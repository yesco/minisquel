CREATE TABLE flights 
	( 
		departure CHAR (10) NOT NULL WITH DEFAULT,   
		arrival CHAR (10) NOT NULL WITH DEFAULT,  
		carrier CHAR (15) NOT NULL WITH DEFAULT, 
		flight_num CHAR (5) NOT NULL WITH DEFAULT,   
		ticket INT NOT NULL WITH DEFAULT)

WITH destinations (departure, arrival, connects, cost ) AS
  (
  SELECT  f.departure,f.arrival, 0, ticket 
  FROM flights f 
  WHERE f.departure = 'Chicago' OR
      f.departure = 'New York'
  UNION ALL
  SELECT
      r.departure, b.arrival, r.connects + 1,
      r.cost + b.ticket 
  FROM  destinations r, flights b 
  WHERE r.arrival = b.departure
  ) 
SELECT DISTINCT departure, arrival, connects, cost 
FROM destinations 


# https://www.ibm.com/docs/en/i/7.4?topic=optimization-example

SELECT DISTINCT departure, arrival, connects, cost 
FROM destinations 
