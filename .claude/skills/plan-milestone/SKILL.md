---
description: Plans a high-level milestone, split it into small GitHub issues
disable-model-invocation: true
argument-hint: <milestone ID>
---

Plan the following feature:

!`gh milestone view "$ARGUMENTS"`

## Requirements

* Ask questions if some aspects are not clear enough
* Split the high-level feature into small tasks
* Add rationale for high-level decisions and architecture
* Add rationale for every small task
* Propose a list of GitHub issues that will be implemented to complete the milestone
* Ask user confirmation before creating any GitHub issue

## GitHub issues format

* The issues will be read by you for implementation, be specific enough
* Issues should be linked to the milestone
* Add task type labels to issues: `feature` for features and `bug` for bugfixes
* Add impacted module labels to issues: `core`, `video`, `renderer`, etc.
