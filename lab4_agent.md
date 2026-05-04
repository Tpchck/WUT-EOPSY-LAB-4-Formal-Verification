---
tags: [eops, lab, sem6, agent-instructions]
created: 2026-05-02
domain: learning
---
# Lab 4 — Agent Instructions (Formal Verification)

## Контекст

Это EOPS Lab 4 в WUT. Задача: формально верифицировать синхронизацию из Lab 3 (producer-consumer с POSIX semaphores) на отсутствие communication deadlock с помощью инструментов **Rybu** и **DedAn**.

---

## Структура папки (уже существует)

```
lab4 - formal verification/
  DedAn/
    rybu.exe          — препроцессор Rybu → DedAn
    DedAn.exe         — верификатор (скачан пользователем)
    Rybu_comand_line.bat — пример запуска
  examples/
    server_view/      — примеры на Rybu: BufS.txt, semS.txt, phil3S.txt и др.
    agent_view/       — то что генерирует rybu.exe (DedAn-формат)
    EXAMPLES.txt      — описание всех примеров
  Rybu - Imperative-style Preprocessor...pdf  — спецификация языка Rybu
  lab4.md             — пользовательский чеклист
  lab4_agent.md       — этот файл
```

---

## Система из Lab 3 (что нужно смоделировать)

**Файлы Lab 3:** `../lab3 - sync/WUT-EOPSY-LAB-3-Synchronization/`
Ключевые: `workers.cpp`, `shared_data.h`, `queue.cpp`

**Процессы:**
- 3 producer: генерируют элементы типа A, B, C
- 2 consumer:
  - C1: берёт один A или B, когда в очереди **нет** C
  - C2: берёт тройку (A+B+C), или одиночный C когда (нет A и B) или (очередь полна)

**Семафоры в реальном коде:**
- `mutex` — binary, critical section
- `slots` — counting (capacity 16), producer ждёт если очередь полна
- `wake_c1`, `wake_c2` — binary, целевое пробуждение конкретного consumer после каждого dispatch

**Dispatch-логика** (вызывается после каждой модификации очереди):
```
if (count_a > 0 || count_b > 0) && count_c == 0  → post(wake_c1)
if count_c > 0 && (count_a > 0 && count_b > 0)   → post(wake_c2)
if count_c > 0 && count_a == 0 && count_b == 0   → post(wake_c2)
if count_c > 0 && count >= CAPACITY               → post(wake_c2)
```

---

## Требования к модели

### Абстракция (обязательно)
Полная модель (16 слотов × типы × значения) имеет взрывной state space.
Необходимо:
- Очередь = абстрактные счётчики `(na, nb, nc)` — содержимое не важно
- `CAPACITY = 4` (вместо 16)
- Каждый producer производит ровно **1 элемент** (минимальный сценарий)

Логика dispatch и семафоров при этом сохраняется полностью.

### Что должна доказать верификация
1. Основная модель (с правильной синхронизацией) → **no deadlock**
2. Broken-версия (например, убрать mutex или нарушить порядок wake) → **deadlock найден**

---

## Синтаксис Rybu (server-view)

Изучи `examples/server_view/BufS.txt` и `semS.txt` — они ближайшие аналоги.

Шаблон структуры:
```
system  SYSTEM_NAME;

#DEFINE CAPACITY 4

server: server_name (agents AgentType[N]; servers OtherServer[M]),
services  {svc1, svc2},
states    {state1, state2},
actions {
    {Agent[i].server_name.svc1, server_name.state1}
        -> {Agent[i].OtherServer[i].svc2, server_name.state2},
    <i=1..N>{...}  // макрогенерация
};

agent: AgentType;
agents  AgentA, AgentB;
servers srv1, srv2;

init -> {
    srv1(AgentA, AgentB).state1,
    AgentA.srv1.svc1,
};
```

Ключевые правила:
- Каждое `action`: `{входящее_сообщение, текущее_состояние} -> {исходящее_сообщение, новое_состояние}`
- `<j=1..N>{...}` — цикл по j, генерирует N action-ов
- В `init` — начальные состояния серверов + первые сообщения агентов
- Semaphore binary: states `{up, down}`, services `{wait, signal}`
- Semaphore counting: states `{cnt0, cnt1, ..., cntN}`, services `{wait, signal}`

---

## Что нужно создать

### Файл 1: `mymodel.txt` (основная модель — no deadlock)

Серверы:
- `mutex` — binary semaphore
- `slots` — counting semaphore 0..CAPACITY
- `wake_c1`, `wake_c2` — binary semaphores (для пробуждения consumers)
- `dispatch` — сервер, инкапсулирующий dispatch-логику после каждой операции очереди
- `queue_state` — абстрактное состояние очереди: состояния = все комбинации `(na, nb, nc)` при `na+nb+nc <= CAPACITY`

Threads (реализуются как серверы с одним агентом):
- `prod_thread_a`, `prod_thread_b`, `prod_thread_c`
- `cons1_thread`, `cons2_thread`

Агенты: по одному на каждый thread.

### Файл 2: `mymodel_broken.txt` (версия с deadlock для демонстрации)

Минимальное изменение которое приводит к deadlock — например:
- mutex не отпускается перед ожиданием wake (вложенный wait на mutex внутри CS)
- или: оба consumer ждут на одном семафоре перекрёстно

---

## Конвейер запуска

```bat
cd DedAn
rybu.exe < ..\mymodel.txt > ..\mymodel.dedan.txt
rybu.exe < ..\mymodel_broken.txt > ..\mymodel_broken.dedan.txt
```

Затем пользователь запускает DedAn.exe руками через cmd или GUI.

---

## Deliverable

После работы в папке должны быть:
- `mymodel.txt` — Rybu source, рабочая синхронизация
- `mymodel.dedan.txt` — сгенерированный DedAn-файл
- `mymodel_broken.txt` — версия с намеренным deadlock
- `mymodel_broken.dedan.txt` — её DedAn-представление
- Комментарии в `.txt` файлах объясняющие что каждый сервер моделирует

---

## Справочные ресурсы в папке

- `DedAn/DedAn_files.txt` — список файлов на сайте курса
- `examples/EXAMPLES.txt` — описание всех примеров
- `examples/server_view/BufS.txt` — bounded buffer (ближайший аналог)
- `examples/server_view/semS.txt` — два семафора со скрещенным порядком (deadlock)
- `examples/server_view/sem_noDeadlockS.txt` — то же, без deadlock
- `Rybu - Imperative-style Preprocessor...pdf` — полная спецификация языка
