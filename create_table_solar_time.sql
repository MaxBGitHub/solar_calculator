CREATE TABLE solar_time (
	day 				DATE 		PRIMARY KEY,
	sunrise 			DATETIME 	NOT NULL,
	sunset 				DATETIME 	NOT NULL,
	latitude 			DOUBLE 		NOT NULL,
	longitude 			DOUBLE 		NOT NULL,
	[offset] 			VARCHAR(3) 	NOT NULL,
	daylight_savings    BOOLEAN 	NOT NULL 
									DEFAULT 0
);
