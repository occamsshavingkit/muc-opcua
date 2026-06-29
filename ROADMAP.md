# Micro-OPCUA Roadmap

This roadmap outlines the planned feature milestones and maintenance tasks for the `micro-opcua` project. Our goal is to balance the addition of advanced OPC UA features while maintaining our strictly bounded, zero-heap architecture.

> **Note:** Between major feature blocks, we explicitly schedule **Documentation Audits** to ensure that all guides, API references, size ledgers, and conformance matrices stay perfectly synced with the codebase.

---

## 🟢 Phase 1: Core & Embedded Profile Completion (Completed)
*Features 001 through 011.*
- [x] **Nano Profile**: Core Server Facet, UA-TCP, UA-Binary, Address Space Base.
- [x] **Micro Profile**: DataChange Subscriptions, MonitoredItems.
- [x] **Embedded 2017 Profile**: `Basic256Sha256` Security Policy, X509 authentication, Multiple client connections, Method Calls.
- [x] **Full-Featured Enhancements**: Write Service, Alarms & Conditions (Events), Server Diagnostics, Dynamic Nodes.

---

## 🟡 Phase 2: PubSub and Advanced Security (Up Next)
*The immediate next steps focus on introducing IoT-friendly PubSub and expanding our cryptographic footprint.*

- [x] **Feature 012: OPC UA PubSub (Part 14) - UADP / UDP**
  - Introduce connectionless Publish-Subscribe (UADP) over UDP.
  - Ideal for ultra-low footprint sensors broadcasting data without TCP overhead.
- [x] **Feature 013: Advanced Security Policies**
  - Implement `Aes128-Sha256-RsaOaep` and `Aes256-Sha256-RsaPss`.
  - Add basic TrustList management for rejecting unknown client certificates.

**🔍 Milestone Audit A: Documentation & Size Ledger Audit**
*Ensure PubSub and advanced security haven't bloated the `nano` core, update integration guides.*

---

## 🔵 Phase 3: Advanced Address Space Management
*Bringing full dynamic control of the address space to the client (where authorized).*

- [x] **Feature 014: NodeManagement Services**
  - Implement `AddNodes`, `DeleteNodes`, `AddReferences`, and `DeleteReferences`.
  - Allow authorized clients to dynamically build the address space at runtime.
- [x] **Feature 015: Query Services**
  - Implement `QueryFirst` and `QueryNext` to allow clients to search the address space using complex filters.
- [x] **Feature 016: Advanced Alarms & Conditions**
  - Expand the basic event system to support Acknowledgeable Conditions, Active/Inactive states, and Dialogs.

**🔍 Milestone Audit B: Documentation & Memory Model Audit**
*Verify that dynamic NodeManagement adheres to the zero-heap constraints (e.g., using a fixed-size pre-allocated node pool).*

---

## 🟣 Phase 4: Historical Data & Enterprise Features
*Expanding into the History Server facet.*

- [x] **Feature 017: Historical Access (HA)**
  - Implement `HistoryRead` (Raw and Modified).
  - Implement `HistoryUpdate` (Insert/Replace/Delete).
  - Provide a platform-agnostic persistence adapter interface (e.g., to write to SD card, flash, or host filesystem).
- [x] **Feature 018: Aggregate Subscriptions**
  - Support for calculating averages, mins, maxes over time periods during publishing.

**🔍 Milestone Audit C: Final API & Conformance Audit**
*Complete review of `docs/api-reference.md` and conformance matrices for HA.*

---

## How we work (SpecKit)
We drive development using the **SpecKit** agentic workflow:
1. `speckit-specify`: Define the natural language requirements.
2. `speckit-plan`: Generate the technical design and memory implications.
3. `speckit-tasks`: Break the plan into sequential, actionable steps.
4. `speckit-implement`: Execute the code generation and tests.
5. `speckit-analyze`: Non-destructive cross-check of the work.
