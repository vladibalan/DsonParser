# DsonParser - Implementer Calibration Log

The staffing calibration log (`Docs/Staffing.md`): a row is appended **at authoring** (predicted
tier + forecast cycles) and completed **at close** (agent, actual cycles from the feedback build
digest, outcome). A handful of rows turns tiering from intuited into calibrated. Cold/on-demand.

| Date | Task `<id>` | Predicted tier | Forecast cycles | Agent | Actual cycles | Outcome |
| --- | --- | --- | --- | --- | --- | --- |
