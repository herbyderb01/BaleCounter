# BaleCounter

## How Bales per hour is calculated

### Key Features:

**Timestamp Tracking**: The system now stores timestamps of when each bale is counted in a circular buffer that can hold up to 100 recent bale timestamps.

**Rolling Window Calculation**: The calculateBalesPerHour() function calculates the rate based on:

- If you have a full hour of data: it counts bales from the last hour
- If you have less than an hour: it calculates the rate based on available time and extrapolates to hourly rate

**Automatic Updates**:

- The bales per hour is recalculated every time a new bale is detected
- It also updates every 30 seconds in the main loop to refresh the display
- Old timestamps (older than 2 hours) are automatically cleaned up

**Display Integration**: The calculated rate is displayed in the uiCYD_BaleCountHour UI element with one decimal place precision.

### How It Works:

- When a bale is detected: The addBaleTimestamp() function is called, which stores the current time, cleans up old data, recalculates the rate, and updates the display
- Rate calculation: Uses a smart algorithm that can work with partial hour data early in the session and gives accurate hourly rates once enough data is collected
- Memory efficient: Uses a circular buffer to prevent memory overflow and automatically cleans up old data

The bales per hour will show "0.0" initially and will become more accurate as more bales are processed. The rate will be most accurate after you've been running for at least an hour, but it will show meaningful extrapolated rates even with just a few bales detected.