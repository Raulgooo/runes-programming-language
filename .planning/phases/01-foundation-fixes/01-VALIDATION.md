---
phase: 1
slug: foundation-fixes
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-02
---

# Phase 1 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | bash test runner (tester.bash) + manual C assert tests |
| **Config file** | `src/tests/tester.bash` |
| **Quick run command** | `bash src/tests/tester.bash` |
| **Full suite command** | `bash src/tests/tester.bash` |
| **Estimated runtime** | ~2 seconds |

---

## Sampling Rate

- **After every task commit:** Run `bash src/tests/tester.bash`
- **After every plan wave:** Run `bash src/tests/tester.bash`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 1-01-01 | 01 | 1 | FOUND-01 | integration | `bash src/tests/tester.bash` | ✅ | ⬜ pending |
| 1-01-02 | 01 | 1 | FOUND-02 | integration | `bash src/tests/tester.bash` | ✅ | ⬜ pending |
| 1-01-03 | 01 | 1 | FOUND-03 | integration | `bash src/tests/tester.bash` | ✅ | ⬜ pending |
| 1-01-04 | 01 | 1 | FOUND-04 | integration | `bash src/tests/tester.bash` | ✅ | ⬜ pending |
| 1-01-05 | 01 | 1 | FOUND-05 | build | `make 2>&1 \| grep warning` | ✅ | ⬜ pending |
| 1-01-06 | 01 | 1 | FOUND-06 | integration | `bash src/tests/tester.bash` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. The test runner (`tester.bash`) needs enhancement for expected-failure tests but this is part of FOUND-06.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| ICE message format | FOUND-01 | Requires crafting a program that triggers TY_UNKNOWN reaching validation | Create a test file with an expression that fails inference, verify stderr contains "internal error" |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
