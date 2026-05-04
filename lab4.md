---
tags: [eops, lab, sem6, university, formal-verification]
created: 2026-05-02
domain: learning
---

# EOPS Lab 4 — Formal Verification

## Что это

Формальная верификация синхронизации из Lab 3 (producer-consumer) на отсутствие deadlock.
Агент пишет модель на языке **Rybu**, прогоняет через **DedAn** — верификатор.

---

## Что тебе нужно сделать руками

### 1. Скачать DedAn

Сайт: `https://staff.elka.pw.edu.pl/~wdaszczu/`
Скачать: `DedAn.exe` + все `.dll` рядом.
Положить в папку `DedAn/` (rybu.exe уже там есть).

### 2. Запустить верификацию (после того как агент напишет модель)

```bat
cd "путь\до\lab4 - formal verification\DedAn"
rybu.exe < ..\mymodel.txt > ..\mymodel.dedan.txt
DedAn.exe < ..\mymodel.dedan.txt
```

Или открыть `Rybu_comand_line.bat` как шаблон и адаптировать под свой файл.

### 3. Прочитать вывод DedAn

- `No deadlock` — синхронизация корректна.
- `DEADLOCK` — что-то не так, смотреть trace.

---

## Чекбокс прогресса

- [x] DedAn.exe скачан, лежит в `DedAn/` ✅ 2026-05-02
- [x] Агент написал `mymodel.dedan.txt` (server_view, 10 серверов) ✅ 2026-05-03
- [x] DedAn дал результат: **together=ON → все Ok** (no deadlock) ✅ 2026-05-03
- [ ] Broken-версия проверена (`mymodel_broken.dedan.txt` написана, ожидает запуска)
- [ ] Результаты зафиксированы

---

## Примечания

- Примеры для изучения синтаксиса: `examples/server_view/BufS.txt`, `semS.txt`
- PDF с описанием Rybu: `Rybu - Imperative-style Preprocessor...pdf` (в корне lab4)
- Агент работает строго по `lab4_agent.md` рядом с этим файлом
