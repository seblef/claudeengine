---
argument-hint: <issue-number>
---

Implement a feature following the requirements in the provided GitHub issue.

!`gh issue view $ARGUMENTS`

Read and implement the issue above.

1. **CRITICAL** Always checkout `dev` and pull last changes before anything else. Never checkout `main`, features always start from latest `dev` commit.
2. Create a branch meaningfully named and prefix by `feat/`
3. Checkout the created branch
4. Implement
5. Run `cpplint` to ensure code is properly formated
6. Commit with message matching conventional commits
7. Open a Pull Request to `dev` with meaningful description and GitHub issue closing command (`Close #[ISSUE_ID]`)
