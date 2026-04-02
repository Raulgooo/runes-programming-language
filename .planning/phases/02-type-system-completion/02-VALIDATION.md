---
phase: 02
slug: type-system-completion
status: draft
nyquist_compliant: false
wave_0_complete: false
created: 2026-04-02
---

# Phase 02 — Validation Strategy

> Per-phase validation contract for feedback sampling during execution.

---

## Test Infrastructure

| Property | Value |
|----------|-------|
| **Framework** | Custom bash test runner (tester.bash) |
| **Config file** | `src/tests/tester.bash` |
| **Quick run command** | `make clean && make 2>&1 && bash src/tests/tester.bash` |
| **Full suite command** | `make clean && make 2>&1 && bash src/tests/tester.bash` |
| **Estimated runtime** | ~5 seconds |

---

## Sampling Rate

- **After every task commit:** Run `make && bash src/tests/tester.bash`
- **After every plan wave:** Run `make clean && make 2>&1 && bash src/tests/tester.bash`
- **Before `/gsd:verify-work`:** Full suite must be green
- **Max feedback latency:** 5 seconds

---

## Per-Task Verification Map

| Task ID | Plan | Wave | Requirement | Test Type | Automated Command | File Exists | Status |
|---------|------|------|-------------|-----------|-------------------|-------------|--------|
| 02-01-01 | 01 | 1 | TYPE-01/02/03/04 | integration | `make && bash src/tests/tester.bash` | ✅ | ⬜ pending |
| 02-02-01 | 02 | 1 | TYPE-05/06/10 | integration | `make && bash src/tests/tester.bash` | ✅ | ⬜ pending |
| 02-03-01 | 03 | 2 | TYPE-07/08/09/11 | integration | `make && bash src/tests/tester.bash` | ✅ | ⬜ pending |

*Status: ⬜ pending · ✅ green · ❌ red · ⚠️ flaky*

---

## Wave 0 Requirements

Existing infrastructure covers all phase requirements. The bash test runner with expected-failure support (from Phase 1) handles both positive and negative test cases.

---

## Manual-Only Verifications

| Behavior | Requirement | Why Manual | Test Instructions |
|----------|-------------|------------|-------------------|
| Error message wording quality | TYPE-01 through TYPE-11 | Subjective readability | Review error output for clarity and helpfulness |

---

## Validation Sign-Off

- [ ] All tasks have `<automated>` verify or Wave 0 dependencies
- [ ] Sampling continuity: no 3 consecutive tasks without automated verify
- [ ] Wave 0 covers all MISSING references
- [ ] No watch-mode flags
- [ ] Feedback latency < 5s
- [ ] `nyquist_compliant: true` set in frontmatter

**Approval:** pending
