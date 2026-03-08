# Malla

Terminal-based PostgreSQL browser and editor. Navigate databases, filter rows, and edit cells without leaving the terminal.

## Screenshot

```
┌─ mydb ─────┐┌─ users ────────┐┌─ Columns ──────────┐┌─ Filter ────────────────────────────┐
│ mydb       ││ orders         ││ [x] id             ││ age > 25                            │
└────────────┘│ products       ││ [x] name           │└─────────────────────────────────────┘
              │ users          ││ [x] email          │
              └────────────────┘│ [ ] created_at     │
                                └────────────────────┘
┌───────────────────────────────────────────────────────────────────────────────────────────┐
│ id             name               email                                                   │
│───────────────────────────────────────────────────────────────────────────────────────────│
│ 1              Alice              alice@example.com                                       │
│                                                                                           │
│ 2              Bob                bob@example.com                                         │
│                                                                                           │
│ 3              Carol              carol@example.com                                       │
│                                                                                           │
└───────────────────────────────────────────────────────────────────────────────────────────┘
```

## Features

- Browse all PostgreSQL databases and tables from searchable dropdowns
- Toggle visible columns with a multi-select dropdown
- Filter rows with a SQL WHERE condition (e.g. `age > 25`, `name LIKE 'A%'`)
- Scroll the data grid vertically and horizontally with arrow keys or mouse wheel
- Edit any cell inline — confirm with Enter, cancel with Escape
- Fixed column header that stays visible while scrolling

## Usage

```bash
malla [options]
```

| Flag | Description |
|------|-------------|
| `-H` | Host (default: `localhost`) |
| `-P` | Port (default: `5432`) |
| `-N` | Database name |
| `-T` | Table name |
| `-U` | User |
| `-p` | Password |

```bash
# Connect and open directly to a table
malla -H localhost -P 5432 -N mydb -T users -U postgres
```

## Keyboard

| Key | Action |
|-----|--------|
| `Tab` | Toggle focus between header and data table |
| `↑ ↓ ← →` or `hjkl` | Move cursor in data table |
| `Enter` | Edit cell / confirm edit |
| `Escape` | Cancel edit |
| `q` / `Q` | Quit |

### Filter input

| Key | Action |
|-----|--------|
| `Enter` (focused) | Enter edit mode |
| `Enter` (editing) | Run query with WHERE condition |
| `Escape` (editing) | Exit edit mode, keep text |

### Cell editing

When editing starts, the full cell value is selected (shown in blue). Typing replaces it entirely. Arrow keys instead move the cursor within the text.

### Edit confirmation modal

```
┌──────────────────────────────┐
│ Update cell?                 │
│──────────────────────────────│
│ Table:  users                │
│ Column: email                │
│ id:     42                   │
│ Old:    bob@example.com      │
│ New:    bob@newdomain.com    │
│──────────────────────────────│
│ [Enter] Confirm  [Esc] Cancel│
└──────────────────────────────┘
```
