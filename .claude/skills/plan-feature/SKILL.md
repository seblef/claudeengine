---
description: Plans a high-level feature, split it into small GitHub issues
disable-model-invocation: true
argument-hint: <high level feature description>
---

Plan the following feature:

!`echo "$ARGUMENTS"`

## Requirements

* Ask questions if some aspects are not clear enough
* Split the high-level feature into small tasks
* Add rationale for high-level decisions and architecture
* Add rationale for every small task
* Propose a list of GitHub issues
* Ask user confirmation before creating any GitHub issue

## GitHub issues format

* The issues will be read by you for implementation, be specific enough
* Add task type labels to issues: `feature` for features and `bug` for bugfixes
* Add impacted module labels to issues: `core`, `video`, `renderer`, etc.
