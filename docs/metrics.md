# ActivityOS metrics

All analytics are deterministic and derived from locally stored application sessions. Thresholds can be adjusted without changing raw events.

## Core time

- **Active time:** Union of valid non-idle application-session durations.
- **Idle time:** Union of intervals where OS-reported inactivity exceeds the configured threshold.
- **Productive time:** Active time in categories marked productive.
- **Time distribution:** Category duration divided by total active duration.
- **Context switch:** A transition between two different foreground applications.
- **Switch rate:** Context switches divided by active hours.

## Focus and deep work

Default focus criteria:

- Productive category
- At least 20 minutes
- No more than one switch during the session
- Not marked as a distraction

Default deep-work criteria:

- Meets focus criteria
- At least 30 minutes
- No switches during the session

These definitions describe sustained computer activity, not the quality or value of someone’s work.

## Distractions and recovery

A potential distraction is a session explicitly classified as distracting. It is considered contextual when it follows productive activity.

- **Minor:** less than 2 minutes
- **Moderate:** 2–10 minutes
- **Major:** more than 10 minutes
- **Recovery time:** Time after a distraction until the beginning of a sustained productive session.
- **Estimated distraction cost:** Distraction duration plus measured recovery time.

ActivityOS deliberately uses “potential distraction” and does not claim that a transition caused later behavior.

## Baselines and trends

- **Personal baseline:** Average of up to 14 prior complete days, excluding the current day.
- **Daily comparison:** Percentage change from that personal baseline.
- **Consistency:** A normalized measure derived from day-to-day variation in productive time.
- **Peak period:** The half-hour bucket containing the most productive activity.
- **Weekly trend:** Comparable seven-day aggregates ordered chronologically.

Empty days and unavailable denominators produce zero values rather than undefined or infinite output.

## Productivity score

The explainable score is clamped to 0–100. Default contributions are:

- Focus progress: up to 35 points
- Deep-work progress: up to 25 points
- Consistency: up to 15 points
- Distraction: up to a 15-point penalty
- Excessive switching: up to a 10-point penalty

The dashboard shows each component. This is a configurable reflection tool, not an employee-performance rating or universal measure of productivity.

## Workstyle profile

The profile summarizes observed patterns such as typical sustained-session length, peak period, switch level, distraction sensitivity, recovery time, and strongest observed environment. It updates as the user’s history changes and compares the user only with their own data.
