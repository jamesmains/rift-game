import os
import sys
import shutil
import zipfile
from datetime import datetime
import tkinter as tk
from tkinter import ttk, filedialog, messagebox, simpledialog

_HOME = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # the project folder (C:/Apps/Triffall)
DEFAULT_DATA_DIR = os.path.join(_HOME, "src", "assets", "data")
DEFAULT_BACKUP_DIR = os.path.join(_HOME, "data-backups")


class FormatParser:
    """
    Dynamically detects and parses Riftfall plain-text data formats:
    1. INI / Section Block Format: [node_id] followed by key=value lines (allowing duplicate keys like 'option=')
    2. Key=Value Flat Format: key=value lines without sections
    3. Delimited Table Format: Pipe (|) or Tab/Comma separated rows
    4. Raw Text / Lore / Map Grid Format
    """

    @staticmethod
    def parse(text, category="", filename=""):
        lines = text.splitlines()
        non_empty = [l.strip() for l in lines if l.strip() and not l.strip().startswith(("#", "//", ";"))]

        if not non_empty:
            return {"mode": "raw", "raw": text}

        # Check for [section] format (used in dialogue/*.txt and structured entity files)
        has_sections = any(l.startswith("[") and l.endswith("]") for l in non_empty)
        has_kv = any("=" in l for l in non_empty if not (l.startswith("[") and l.endswith("]")))

        if has_sections and has_kv:
            return FormatParser._parse_sections(lines)

        # Check if flat Key=Value format
        kv_count = sum(1 for l in non_empty if "=" in l and not l.startswith("["))
        if kv_count > len(non_empty) * 0.6:
            return FormatParser._parse_flat_kv(lines)

        # Check if Pipe-delimited table format (common in RPG data files)
        pipe_lines = [l for l in non_empty if "|" in l]
        if len(pipe_lines) >= 2 and len(pipe_lines) > len(non_empty) * 0.5:
            return FormatParser._parse_delimited(lines, "|")

        # Fallback to Raw/Lore/Grid mode
        return {"mode": "raw", "raw": text}

    @staticmethod
    def _parse_sections(lines):
        sections = []
        current_sec = None
        preamble = []

        for line in lines:
            stripped = line.strip()
            if stripped.startswith("[") and stripped.endswith("]"):
                sec_name = stripped[1:-1]
                current_sec = {"name": sec_name, "entries": []}
                sections.append(current_sec)
            elif current_sec is not None:
                if "=" in line and not stripped.startswith(("#", "//", ";")):
                    k, v = line.split("=", 1)
                    current_sec["entries"].append({"type": "kv", "key": k.strip(), "value": v.strip()})
                else:
                    current_sec["entries"].append({"type": "raw", "text": line})
            else:
                preamble.append(line)

        return {"mode": "sections", "preamble": preamble, "sections": sections}

    @staticmethod
    def serialize_sections(data):
        out = []
        for p in data.get("preamble", []):
            out.append(p)
        for sec in data.get("sections", []):
            out.append(f"[{sec['name']}]")
            for entry in sec["entries"]:
                if entry["type"] == "kv":
                    out.append(f"{entry['key']}={entry['value']}")
                else:
                    out.append(entry["text"])
        return "\n".join(out)

    @staticmethod
    def _parse_flat_kv(lines):
        entries = []
        for line in lines:
            stripped = line.strip()
            if "=" in line and not stripped.startswith(("#", "//", ";")):
                k, v = line.split("=", 1)
                entries.append({"type": "kv", "key": k.strip(), "value": v.strip()})
            else:
                entries.append({"type": "raw", "text": line})
        return {"mode": "flat_kv", "entries": entries}

    @staticmethod
    def serialize_flat_kv(data):
        out = []
        for entry in data.get("entries", []):
            if entry["type"] == "kv":
                out.append(f"{entry['key']}={entry['value']}")
            else:
                out.append(entry["text"])
        return "\n".join(out)

    @staticmethod
    def _parse_delimited(lines, delimiter="|"):
        rows = []
        max_cols = 0
        for line in lines:
            stripped = line.strip()
            if not stripped or stripped.startswith(("#", "//", ";")):
                rows.append({"type": "raw", "text": line})
            else:
                cols = [c.strip() for c in line.split(delimiter)]
                max_cols = max(max_cols, len(cols))
                rows.append({"type": "row", "cols": cols})
        return {"mode": "delimited", "delimiter": delimiter, "max_cols": max_cols, "rows": rows}

    @staticmethod
    def serialize_delimited(data):
        delim = data.get("delimiter", "|")
        out = []
        for r in data.get("rows", []):
            if r["type"] == "row":
                out.append(delim.join(r["cols"]))
            else:
                out.append(r["text"])
        return "\n".join(out)


class RiftfallDataStudio(tk.Tk):
    def __init__(self):
        super().__init__()
        self.title("Riftfall Data Studio & Backup Manager (Build 0.7)")
        self.geometry("1280x820")
        self.minsize(1024, 700)

        self.data_dir = tk.StringVar(value=DEFAULT_DATA_DIR)
        self.backup_dir = tk.StringVar(value=DEFAULT_BACKUP_DIR)

        self.current_file_rel = None
        self.current_parsed = None
        self.is_dirty = False

        self._setup_styles()
        self._build_ui()
        self.refresh_file_tree()
        self.refresh_backup_list()

    def _setup_styles(self):
        style = ttk.Style(self)
        if "vista" in style.theme_names():
            style.theme_use("vista")
        elif "clam" in style.theme_names():
            style.theme_use("clam")
        style.configure("Treeview", rowheight=24, font=("Segoe UI", 10))
        style.configure("Treeview.Heading", font=("Segoe UI", 10, "bold"))
        style.configure("Header.TLabel", font=("Segoe UI", 11, "bold"))

    def _build_ui(self):
        # Top Configuration Bar (Target Data & Backups Directories)
        top_frame = ttk.LabelFrame(self, text="Directory Targets & Quick Actions", padding=8)
        top_frame.pack(fill=tk.X, padx=8, pady=6)

        ttk.Label(top_frame, text="Data Directory:").grid(row=0, column=0, sticky=tk.W, padx=4, pady=2)
        ttk.Entry(top_frame, textvariable=self.data_dir, width=75).grid(row=0, column=1, sticky=tk.EW, padx=4, pady=2)
        ttk.Button(top_frame, text="Browse...", command=self.browse_data_dir).grid(row=0, column=2, padx=4, pady=2)
        ttk.Button(top_frame, text="Reload Data", command=self.refresh_file_tree).grid(row=0, column=3, padx=4, pady=2)

        ttk.Label(top_frame, text="Backups Directory:").grid(row=1, column=0, sticky=tk.W, padx=4, pady=2)
        ttk.Entry(top_frame, textvariable=self.backup_dir, width=75).grid(row=1, column=1, sticky=tk.EW, padx=4, pady=2)
        ttk.Button(top_frame, text="Browse...", command=self.browse_backup_dir).grid(row=1, column=2, padx=4, pady=2)
        ttk.Button(top_frame, text="Create Backup Now", command=self.create_backup).grid(row=1, column=3, padx=4, pady=2)

        top_frame.columnconfigure(1, weight=1)

        # Main PanedWindow
        main_pane = ttk.PanedWindow(self, orient=tk.HORIZONTAL)
        main_pane.pack(fill=tk.BOTH, expand=True, padx=8, pady=4)

        # Left Sidebar: File Category Tree
        left_frame = ttk.Frame(main_pane, width=280)
        main_pane.add(left_frame, weight=1)

        ttk.Label(left_frame, text="Data Categories & Files", style="Header.TLabel").pack(anchor=tk.W, pady=(0, 4))

        search_frame = ttk.Frame(left_frame)
        search_frame.pack(fill=tk.X, pady=(0, 4))
        ttk.Label(search_frame, text="Filter:").pack(side=tk.LEFT)
        self.filter_var = tk.StringVar()
        self.filter_var.trace_add("write", lambda *args: self.refresh_file_tree())
        ttk.Entry(search_frame, textvariable=self.filter_var).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=4)

        tree_scroll = ttk.Scrollbar(left_frame, orient=tk.VERTICAL)
        self.file_tree = ttk.Treeview(left_frame, yscrollcommand=tree_scroll.set, selectmode="browse")
        self.file_tree.heading("#0", text="Riftfall Data Tree", anchor=tk.W)
        tree_scroll.config(command=self.file_tree.yview)
        tree_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.file_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)
        self.file_tree.bind("<<TreeviewSelect>>", self.on_tree_select)

        # Right Area: Notebook for Editor & Backup Manager
        right_frame = ttk.Frame(main_pane)
        main_pane.add(right_frame, weight=4)

        # File Header & Save Bar
        file_bar = ttk.Frame(right_frame)
        file_bar.pack(fill=tk.X, pady=(0, 6))

        self.lbl_current_file = ttk.Label(file_bar, text="No file selected", style="Header.TLabel")
        self.lbl_current_file.pack(side=tk.LEFT)

        self.lbl_format_badge = ttk.Label(file_bar, text="", foreground="#005fb8")
        self.lbl_format_badge.pack(side=tk.LEFT, padx=12)

        ttk.Button(file_bar, text="Save File (Ctrl+S)", command=self.save_current_file).pack(side=tk.RIGHT, padx=4)
        ttk.Button(file_bar, text="Revert Unsaved Changes", command=self.reload_current_file).pack(side=tk.RIGHT, padx=4)

        # Main Notebook
        self.notebook = ttk.Notebook(right_frame)
        self.notebook.pack(fill=tk.BOTH, expand=True)

        # Tab 1: Category / Visual Structured Editor
        self.tab_visual = ttk.Frame(self.notebook, padding=6)
        self.notebook.add(self.tab_visual, text="Visual Category Editor")

        # Tab 2: Raw Plain-Text Editor
        self.tab_raw = ttk.Frame(self.notebook, padding=6)
        self.notebook.add(self.tab_raw, text="Raw Plain-Text Editor")
        self._build_raw_editor()

        # Tab 3: Backup & Restore Manager
        self.tab_backups = ttk.Frame(self.notebook, padding=10)
        self.notebook.add(self.tab_backups, text="Backup & Replace Manager")
        self._build_backup_manager()

        # Bind Ctrl+S
        self.bind_all("<Control-s>", lambda e: self.save_current_file())

        # Status Bar
        self.status_var = tk.StringVar(value="Ready.")
        status_bar = ttk.Label(self, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W, padding=(6, 2))
        status_bar.pack(fill=tk.X, side=tk.BOTTOM)

    def _build_raw_editor(self):
        toolbar = ttk.Frame(self.tab_raw)
        toolbar.pack(fill=tk.X, pady=(0, 4))
        ttk.Label(toolbar, text="Direct Plain-Text Editor (Changes sync automatically on Save or Tab Switch)").pack(side=tk.LEFT)
        ttk.Button(toolbar, text="Re-Parse Raw Text into Visual Editor", command=self.sync_raw_to_visual).pack(side=tk.RIGHT)

        text_frame = ttk.Frame(self.tab_raw)
        text_frame.pack(fill=tk.BOTH, expand=True)

        yscroll = ttk.Scrollbar(text_frame, orient=tk.VERTICAL)
        xscroll = ttk.Scrollbar(text_frame, orient=tk.HORIZONTAL)
        self.raw_text = tk.Text(
            text_frame, wrap=tk.NONE, font=("Consolas", 10), undo=True,
            yscrollcommand=yscroll.set, xscrollcommand=xscroll.set
        )
        yscroll.config(command=self.raw_text.yview)
        xscroll.config(command=self.raw_text.xview)
        yscroll.pack(side=tk.RIGHT, fill=tk.Y)
        xscroll.pack(side=tk.BOTTOM, fill=tk.X)
        self.raw_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    def _build_backup_manager(self):
        top_info = ttk.LabelFrame(self.tab_backups, text="Backup & Data Replacement Operations", padding=10)
        top_info.pack(fill=tk.X, pady=(0, 10))

        btn_row = ttk.Frame(top_info)
        btn_row.pack(fill=tk.X)

        ttk.Button(btn_row, text="1. Create Full Timestamped Backup (.zip)", command=self.create_backup).pack(side=tk.LEFT, padx=4)
        ttk.Button(btn_row, text="2. Restore & Replace Live Data from Selected Backup", command=self.restore_selected_backup).pack(side=tk.LEFT, padx=4)
        ttk.Button(btn_row, text="3. Import External ZIP / Folder to Replace Data", command=self.import_external_data).pack(side=tk.LEFT, padx=4)
        ttk.Button(btn_row, text="Delete Selected Backup", command=self.delete_selected_backup).pack(side=tk.RIGHT, padx=4)

        list_frame = ttk.LabelFrame(self.tab_backups, text="Available Backups in Backup Directory", padding=8)
        list_frame.pack(fill=tk.BOTH, expand=True)

        cols = ("filename", "date", "size")
        self.backup_tree = ttk.Treeview(list_frame, columns=cols, show="headings", selectmode="browse")
        self.backup_tree.heading("filename", text="Backup Archive Name")
        self.backup_tree.heading("date", text="Created Timestamp")
        self.backup_tree.heading("size", text="Size (KB)")
        self.backup_tree.column("filename", width=360)
        self.backup_tree.column("date", width=200)
        self.backup_tree.column("size", width=120, anchor=tk.E)

        b_scroll = ttk.Scrollbar(list_frame, orient=tk.VERTICAL, command=self.backup_tree.yview)
        self.backup_tree.configure(yscrollcommand=b_scroll.set)
        b_scroll.pack(side=tk.RIGHT, fill=tk.Y)
        self.backup_tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

    # -------------------------------------------------------------------------
    # Directory & File Tree Management
    # -------------------------------------------------------------------------
    def browse_data_dir(self):
        d = filedialog.askdirectory(initialdir=self.data_dir.get(), title="Select Riftfall Data Directory")
        if d:
            self.data_dir.set(os.path.normpath(d))
            self.refresh_file_tree()

    def browse_backup_dir(self):
        d = filedialog.askdirectory(initialdir=self.backup_dir.get(), title="Select Backups Directory")
        if d:
            self.backup_dir.set(os.path.normpath(d))
            self.refresh_backup_list()

    def refresh_file_tree(self):
        self.file_tree.delete(*self.file_tree.get_children())
        base = self.data_dir.get()
        flt = self.filter_var.get().lower().strip()

        if not os.path.isdir(base):
            self.status_var.set(f"Data directory not found: {base}. Use 'Browse...' or 'Import External ZIP' to load.")
            return

        # Categories nodes
        cat_nodes = {
            "core": self.file_tree.insert("", tk.END, text="Core Game Tables (root)", open=True),
            "dialogue": self.file_tree.insert("", tk.END, text="Dialogue Trees (dialogue/)", open=True),
            "lore": self.file_tree.insert("", tk.END, text="Lore Documents (lore/)", open=True),
            "maps": self.file_tree.insert("", tk.END, text="World & Region Maps (maps/)", open=True),
            "other": None
        }

        count = 0
        for root, dirs, files in os.walk(base):
            dirs.sort()
            for f in sorted(files):
                if not f.lower().endswith(".txt"):
                    continue
                full_path = os.path.join(root, f)
                rel_path = os.path.relpath(full_path, base).replace("\\", "/")
                if flt and flt not in rel_path.lower():
                    continue

                if "/" not in rel_path:
                    parent = cat_nodes["core"]
                elif rel_path.startswith("dialogue/"):
                    parent = cat_nodes["dialogue"]
                elif rel_path.startswith("lore/"):
                    parent = cat_nodes["lore"]
                elif rel_path.startswith("maps/"):
                    parent = cat_nodes["maps"]
                else:
                    if cat_nodes["other"] is None:
                        cat_nodes["other"] = self.file_tree.insert("", tk.END, text="Other Subfolders", open=True)
                    parent = cat_nodes["other"]

                self.file_tree.insert(parent, tk.END, text=f, values=(rel_path,))
                count += 1

        self.status_var.set(f"Loaded {count} data files from {base}")

    def on_tree_select(self, event):
        sel = self.file_tree.selection()
        if not sel:
            return
        vals = self.file_tree.item(sel[0], "values")
        if not vals:
            return  # Category folder clicked
        rel_path = vals[0]
        self.load_file(rel_path)

    def load_file(self, rel_path):
        full_path = os.path.join(self.data_dir.get(), rel_path)
        if not os.path.isfile(full_path):
            messagebox.showerror("File Not Found", f"Could not find:\n{full_path}")
            return

        try:
            with open(full_path, "r", encoding="utf-8", errors="replace") as f:
                content = f.read()
        except Exception as e:
            messagebox.showerror("Read Error", str(e))
            return

        self.current_file_rel = rel_path
        self.lbl_current_file.config(text=f"Editing: {rel_path}")

        # Populate Raw Editor
        self.raw_text.delete("1.0", tk.END)
        self.raw_text.insert("1.0", content)

        # Parse and build Visual Category Editor
        category = rel_path.split("/")[0] if "/" in rel_path else "core"
        self.current_parsed = FormatParser.parse(content, category=category, filename=rel_path)
        self._build_visual_editor_for_current()
        self.status_var.set(f"Opened {rel_path} ({len(content)} bytes)")

    def reload_current_file(self):
        if self.current_file_rel:
            self.load_file(self.current_file_rel)

    # -------------------------------------------------------------------------
    # Visual Category Editors
    # -------------------------------------------------------------------------
    def _build_visual_editor_for_current(self):
        for w in self.tab_visual.winfo_children():
            w.destroy()

        if not self.current_parsed:
            return

        mode = self.current_parsed["mode"]
        rel = self.current_file_rel or ""

        if mode == "sections":
            self.lbl_format_badge.config(text="[Section / Node Block Format]")
            self._build_section_editor(self.tab_visual, self.current_parsed, is_dialogue=rel.startswith("dialogue/"))
        elif mode == "delimited":
            self.lbl_format_badge.config(text=f"[Delimited Table Format ('{self.current_parsed['delimiter']}')]")
            self._build_table_editor(self.tab_visual, self.current_parsed)
        elif mode == "flat_kv":
            self.lbl_format_badge.config(text="[Key=Value Property Format]")
            self._build_flat_kv_editor(self.tab_visual, self.current_parsed)
        else:
            if rel.startswith("maps/") or rel == "overworld.txt":
                self.lbl_format_badge.config(text="[Map / Grid Layout Editor]")
                self._build_map_grid_editor(self.tab_visual, self.current_parsed)
            else:
                self.lbl_format_badge.config(text="[Lore / Freeform Text Document]")
                self._build_lore_editor(self.tab_visual, self.current_parsed)

    # --- 1. Section / Dialogue / Entity Block Editor ---
    def _build_section_editor(self, parent, parsed_data, is_dialogue=False):
        pane = ttk.PanedWindow(parent, orient=tk.HORIZONTAL)
        pane.pack(fill=tk.BOTH, expand=True)

        # Left: List of [sections] (e.g., Dialogue nodes [start] or Entity IDs)
        left = ttk.Frame(pane, width=240)
        pane.add(left, weight=1)

        title = "Dialogue Nodes ([id])" if is_dialogue else "Records / Sections ([id])"
        ttk.Label(left, text=title, style="Header.TLabel").pack(anchor=tk.W, pady=(0, 4))

        btn_bar = ttk.Frame(left)
        btn_bar.pack(fill=tk.X, pady=(0, 4))

        sec_list = tk.Listbox(left, font=("Segoe UI", 10), exportselection=False)
        sec_list.pack(fill=tk.BOTH, expand=True)

        for idx, sec in enumerate(parsed_data["sections"]):
            sec_list.insert(tk.END, f"[{sec['name']}]")

        # Right: Key=Value table for the selected section
        right = ttk.Frame(pane)
        pane.add(right, weight=3)

        sec_header = ttk.Frame(right)
        sec_header.pack(fill=tk.X, pady=(0, 6))
        ttk.Label(sec_header, text="Section ID:").pack(side=tk.LEFT)
        sec_name_var = tk.StringVar()
        ent_sec_name = ttk.Entry(sec_header, textvariable=sec_name_var, width=30)
        ent_sec_name.pack(side=tk.LEFT, padx=6)

        def rename_section(*args):
            sel = sec_list.curselection()
            if not sel:
                return
            idx = sel[0]
            new_name = sec_name_var.get().strip()
            if new_name:
                parsed_data["sections"][idx]["name"] = new_name
                sec_list.delete(idx)
                sec_list.insert(idx, f"[{new_name}]")
                sec_list.selection_set(idx)
                self._sync_visual_to_raw()

        ttk.Button(sec_header, text="Update ID", command=rename_section).pack(side=tk.LEFT)

        # Entries Treeview
        cols = ("key", "value")
        kv_tree = ttk.Treeview(right, columns=cols, show="headings", selectmode="browse")
        kv_tree.heading("key", text="Property / Key (e.g., speaker, text, option)")
        kv_tree.heading("value", text="Value (e.g., Good cat.|end)")
        kv_tree.column("key", width=220, stretch=False)
        kv_tree.column("value", width=520, stretch=True)
        kv_tree.pack(fill=tk.BOTH, expand=True, pady=4)

        # Edit Fields at bottom of right pane
        edit_box = ttk.LabelFrame(right, text="Edit Selected Property / Line", padding=8)
        edit_box.pack(fill=tk.X)

        ttk.Label(edit_box, text="Key:").grid(row=0, column=0, sticky=tk.W, padx=4)
        key_var = tk.StringVar()
        ttk.Entry(edit_box, textvariable=key_var, width=25).grid(row=0, column=1, sticky=tk.W, padx=4)

        ttk.Label(edit_box, text="Value:").grid(row=0, column=2, sticky=tk.W, padx=4)
        val_var = tk.StringVar()
        ttk.Entry(edit_box, textvariable=val_var, width=60).grid(row=0, column=3, sticky=tk.EW, padx=4)
        edit_box.columnconfigure(3, weight=1)

        def refresh_kv_tree(sec_idx):
            kv_tree.delete(*kv_tree.get_children())
            if sec_idx < 0 or sec_idx >= len(parsed_data["sections"]):
                return
            sec = parsed_data["sections"][sec_idx]
            sec_name_var.set(sec["name"])
            for i, entry in enumerate(sec["entries"]):
                if entry["type"] == "kv":
                    kv_tree.insert("", tk.END, iid=str(i), values=(entry["key"], entry["value"]))
                else:
                    kv_tree.insert("", tk.END, iid=str(i), values=("# raw/comment", entry["text"]))

        def on_sec_select(evt):
            sel = sec_list.curselection()
            if sel:
                refresh_kv_tree(sel[0])

        sec_list.bind("<<ListboxSelect>>", on_sec_select)

        def on_kv_select(evt):
            sel = kv_tree.selection()
            if not sel:
                return
            vals = kv_tree.item(sel[0], "values")
            key_var.set(vals[0])
            val_var.set(vals[1])

        kv_tree.bind("<<TreeviewSelect>>", on_kv_select)

        def update_kv():
            s_sel = sec_list.curselection()
            k_sel = kv_tree.selection()
            if not s_sel or not k_sel:
                return
            sec_idx = s_sel[0]
            e_idx = int(k_sel[0])
            k = key_var.get().strip()
            v = val_var.get()
            if k == "# raw/comment":
                parsed_data["sections"][sec_idx]["entries"][e_idx] = {"type": "raw", "text": v}
            else:
                parsed_data["sections"][sec_idx]["entries"][e_idx] = {"type": "kv", "key": k, "value": v}
            refresh_kv_tree(sec_idx)
            kv_tree.selection_set(str(e_idx))
            self._sync_visual_to_raw()

        def add_kv():
            s_sel = sec_list.curselection()
            if not s_sel:
                return
            sec_idx = s_sel[0]
            default_k = "option" if is_dialogue else "property"
            default_v = "Continue.|end" if is_dialogue else "value"
            parsed_data["sections"][sec_idx]["entries"].append({"type": "kv", "key": default_k, "value": default_v})
            refresh_kv_tree(sec_idx)
            self._sync_visual_to_raw()

        def delete_kv():
            s_sel = sec_list.curselection()
            k_sel = kv_tree.selection()
            if not s_sel or not k_sel:
                return
            sec_idx = s_sel[0]
            e_idx = int(k_sel[0])
            del parsed_data["sections"][sec_idx]["entries"][e_idx]
            refresh_kv_tree(sec_idx)
            self._sync_visual_to_raw()

        action_row = ttk.Frame(edit_box)
        action_row.grid(row=1, column=0, columnspan=4, sticky=tk.EW, pady=(6, 0))
        ttk.Button(action_row, text="Apply Changes to Line", command=update_kv).pack(side=tk.LEFT, padx=4)
        ttk.Button(action_row, text="+ Add Property / Option", command=add_kv).pack(side=tk.LEFT, padx=4)
        ttk.Button(action_row, text="- Delete Line", command=delete_kv).pack(side=tk.LEFT, padx=4)

        def add_section():
            new_id = simpledialog.askstring("New Section", "Enter new [section] ID:", parent=self)
            if new_id:
                entries = []
                if is_dialogue:
                    entries = [
                        {"type": "kv", "key": "speaker", "value": "NPC"},
                        {"type": "kv", "key": "text", "value": "..."},
                        {"type": "kv", "key": "option", "value": "Goodbye.|end"}
                    ]
                parsed_data["sections"].append({"name": new_id.strip(), "entries": entries})
                sec_list.insert(tk.END, f"[{new_id.strip()}]")
                sec_list.selection_clear(0, tk.END)
                sec_list.selection_set(tk.END)
                refresh_kv_tree(len(parsed_data["sections"]) - 1)
                self._sync_visual_to_raw()

        def del_section():
            s_sel = sec_list.curselection()
            if not s_sel:
                return
            idx = s_sel[0]
            del parsed_data["sections"][idx]
            sec_list.delete(idx)
            kv_tree.delete(*kv_tree.get_children())
            self._sync_visual_to_raw()

        ttk.Button(btn_bar, text="+ Add Node", command=add_section).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)
        ttk.Button(btn_bar, text="- Delete", command=del_section).pack(side=tk.LEFT, fill=tk.X, expand=True, padx=2)

        if parsed_data["sections"]:
            sec_list.selection_set(0)
            refresh_kv_tree(0)

    # --- 2. Flat Key=Value Editor ---
    def _build_flat_kv_editor(self, parent, parsed_data):
        cols = ("key", "value")
        kv_tree = ttk.Treeview(parent, columns=cols, show="headings", selectmode="browse")
        kv_tree.heading("key", text="Key")
        kv_tree.heading("value", text="Value")
        kv_tree.column("key", width=260, stretch=False)
        kv_tree.column("value", width=650, stretch=True)
        kv_tree.pack(fill=tk.BOTH, expand=True, pady=4)

        edit_box = ttk.LabelFrame(parent, text="Edit Key=Value Entry", padding=8)
        edit_box.pack(fill=tk.X)

        key_var = tk.StringVar()
        val_var = tk.StringVar()
        ttk.Label(edit_box, text="Key:").grid(row=0, column=0, padx=4)
        ttk.Entry(edit_box, textvariable=key_var, width=28).grid(row=0, column=1, padx=4)
        ttk.Label(edit_box, text="Value:").grid(row=0, column=2, padx=4)
        ttk.Entry(edit_box, textvariable=val_var, width=65).grid(row=0, column=3, sticky=tk.EW, padx=4)
        edit_box.columnconfigure(3, weight=1)

        def refresh():
            kv_tree.delete(*kv_tree.get_children())
            for i, e in enumerate(parsed_data["entries"]):
                if e["type"] == "kv":
                    kv_tree.insert("", tk.END, iid=str(i), values=(e["key"], e["value"]))
                else:
                    kv_tree.insert("", tk.END, iid=str(i), values=("# raw/comment", e["text"]))

        def on_select(evt):
            sel = kv_tree.selection()
            if sel:
                v = kv_tree.item(sel[0], "values")
                key_var.set(v[0])
                val_var.set(v[1])

        kv_tree.bind("<<TreeviewSelect>>", on_select)

        def apply_change():
            sel = kv_tree.selection()
            if not sel:
                return
            idx = int(sel[0])
            k = key_var.get().strip()
            v = val_var.get()
            if k == "# raw/comment":
                parsed_data["entries"][idx] = {"type": "raw", "text": v}
            else:
                parsed_data["entries"][idx] = {"type": "kv", "key": k, "value": v}
            refresh()
            kv_tree.selection_set(str(idx))
            self._sync_visual_to_raw()

        def add_row():
            parsed_data["entries"].append({"type": "kv", "key": "new_key", "value": "value"})
            refresh()
            self._sync_visual_to_raw()

        def del_row():
            sel = kv_tree.selection()
            if not sel:
                return
            del parsed_data["entries"][int(sel[0])]
            refresh()
            self._sync_visual_to_raw()

        btns = ttk.Frame(edit_box)
        btns.grid(row=1, column=0, columnspan=4, sticky=tk.W, pady=(6, 0))
        ttk.Button(btns, text="Apply", command=apply_change).pack(side=tk.LEFT, padx=4)
        ttk.Button(btns, text="+ Add Entry", command=add_row).pack(side=tk.LEFT, padx=4)
        ttk.Button(btns, text="- Delete Entry", command=del_row).pack(side=tk.LEFT, padx=4)

        refresh()

    # --- 3. Delimited Table Editor ---
    def _build_table_editor(self, parent, parsed_data):
        max_cols = max(parsed_data.get("max_cols", 1), 1)
        col_ids = [f"col_{i}" for i in range(max_cols)]

        tree_frame = ttk.Frame(parent)
        tree_frame.pack(fill=tk.BOTH, expand=True)

        yscroll = ttk.Scrollbar(tree_frame, orient=tk.VERTICAL)
        xscroll = ttk.Scrollbar(tree_frame, orient=tk.HORIZONTAL)
        tree = ttk.Treeview(
            tree_frame, columns=col_ids, show="headings", selectmode="browse",
            yscrollcommand=yscroll.set, xscrollcommand=xscroll.set
        )
        yscroll.config(command=tree.yview)
        xscroll.config(command=tree.xview)
        yscroll.pack(side=tk.RIGHT, fill=tk.Y)
        xscroll.pack(side=tk.BOTTOM, fill=tk.X)
        tree.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        for i, cid in enumerate(col_ids):
            tree.heading(cid, text=f"Field {i + 1}")
            tree.column(cid, width=140, stretch=True)

        edit_frame = ttk.LabelFrame(parent, text="Edit Selected Row Fields", padding=8)
        edit_frame.pack(fill=tk.X, pady=(6, 0))

        fields_container = ttk.Frame(edit_frame)
        fields_container.pack(fill=tk.X)
        col_vars = [tk.StringVar() for _ in range(max_cols)]

        for i in range(max_cols):
            r = i // 4
            c = (i % 4) * 2
            ttk.Label(fields_container, text=f"F{i + 1}:").grid(row=r, column=c, sticky=tk.W, padx=2, pady=2)
            ttk.Entry(fields_container, textvariable=col_vars[i], width=22).grid(row=r, column=c + 1, sticky=tk.EW, padx=4, pady=2)

        def refresh_table():
            tree.delete(*tree.get_children())
            for idx, r in enumerate(parsed_data["rows"]):
                if r["type"] == "row":
                    padded = r["cols"] + [""] * (max_cols - len(r["cols"]))
                    tree.insert("", tk.END, iid=str(idx), values=padded)

        def on_row_select(evt):
            sel = tree.selection()
            if not sel:
                return
            idx = int(sel[0])
            row_data = parsed_data["rows"][idx]["cols"]
            for i in range(max_cols):
                col_vars[i].set(row_data[i] if i < len(row_data) else "")

        tree.bind("<<TreeviewSelect>>", on_row_select)

        def save_row():
            sel = tree.selection()
            if not sel:
                return
            idx = int(sel[0])
            new_cols = [v.get() for v in col_vars]
            while len(new_cols) > 1 and new_cols[-1] == "":
                new_cols.pop()
            parsed_data["rows"][idx]["cols"] = new_cols
            refresh_table()
            tree.selection_set(str(idx))
            self._sync_visual_to_raw()

        def add_row():
            parsed_data["rows"].append({"type": "row", "cols": ["new_id"] + [""] * (max_cols - 1)})
            refresh_table()
            self._sync_visual_to_raw()

        def delete_row():
            sel = tree.selection()
            if not sel:
                return
            del parsed_data["rows"][int(sel[0])]
            refresh_table()
            self._sync_visual_to_raw()

        btn_bar = ttk.Frame(edit_frame)
        btn_bar.pack(fill=tk.X, pady=(6, 0))
        ttk.Button(btn_bar, text="Apply Row Changes", command=save_row).pack(side=tk.LEFT, padx=4)
        ttk.Button(btn_bar, text="+ Add Row", command=add_row).pack(side=tk.LEFT, padx=4)
        ttk.Button(btn_bar, text="- Delete Selected Row", command=delete_row).pack(side=tk.LEFT, padx=4)

        refresh_table()

    # --- 4. Map / Grid Layout Editor ---
    def _build_map_grid_editor(self, parent, parsed_data):
        info = ttk.Label(
            parent,
            text="Map Layout & Metadata Editor (Monospace Grid View — edits sync directly with file)"
        )
        info.pack(anchor=tk.W, pady=(0, 4))

        frame = ttk.Frame(parent)
        frame.pack(fill=tk.BOTH, expand=True)

        yscroll = ttk.Scrollbar(frame, orient=tk.VERTICAL)
        xscroll = ttk.Scrollbar(frame, orient=tk.HORIZONTAL)
        map_text = tk.Text(
            frame, wrap=tk.NONE, font=("Consolas", 11),
            yscrollcommand=yscroll.set, xscrollcommand=xscroll.set
        )
        yscroll.config(command=map_text.yview)
        xscroll.config(command=map_text.xview)
        yscroll.pack(side=tk.RIGHT, fill=tk.Y)
        xscroll.pack(side=tk.BOTTOM, fill=tk.X)
        map_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        map_text.insert("1.0", parsed_data.get("raw", ""))

        def on_map_modified(event):
            parsed_data["raw"] = map_text.get("1.0", "end-1c")
            self._sync_visual_to_raw()

        map_text.bind("<KeyRelease>", on_map_modified)

    # --- 5. Lore / Freeform Document Editor ---
    def _build_lore_editor(self, parent, parsed_data):
        info = ttk.Label(parent, text="Lore & Narrative Text Editor (Word-Wrapped Reader/Writer View)")
        info.pack(anchor=tk.W, pady=(0, 4))

        frame = ttk.Frame(parent)
        frame.pack(fill=tk.BOTH, expand=True)

        yscroll = ttk.Scrollbar(frame, orient=tk.VERTICAL)
        lore_text = tk.Text(frame, wrap=tk.WORD, font=("Georgia", 11), padx=12, pady=12, yscrollcommand=yscroll.set)
        yscroll.config(command=lore_text.yview)
        yscroll.pack(side=tk.RIGHT, fill=tk.Y)
        lore_text.pack(side=tk.LEFT, fill=tk.BOTH, expand=True)

        lore_text.insert("1.0", parsed_data.get("raw", ""))

        def on_lore_modified(event):
            parsed_data["raw"] = lore_text.get("1.0", "end-1c")
            self._sync_visual_to_raw()

        lore_text.bind("<KeyRelease>", on_lore_modified)

    # -------------------------------------------------------------------------
    # Synchronization & Saving
    # -------------------------------------------------------------------------
    def _sync_visual_to_raw(self):
        if not self.current_parsed:
            return
        mode = self.current_parsed["mode"]
        if mode == "sections":
            text = FormatParser.serialize_sections(self.current_parsed)
        elif mode == "flat_kv":
            text = FormatParser.serialize_flat_kv(self.current_parsed)
        elif mode == "delimited":
            text = FormatParser.serialize_delimited(self.current_parsed)
        else:
            text = self.current_parsed.get("raw", "")

        self.raw_text.delete("1.0", tk.END)
        self.raw_text.insert("1.0", text)

    def sync_raw_to_visual(self):
        if not self.current_file_rel:
            return
        raw_content = self.raw_text.get("1.0", "end-1c")
        cat = self.current_file_rel.split("/")[0] if "/" in self.current_file_rel else "core"
        self.current_parsed = FormatParser.parse(raw_content, category=cat, filename=self.current_file_rel)
        self._build_visual_editor_for_current()
        self.status_var.set("Re-parsed raw text into Visual Editor.")

    def save_current_file(self):
        if not self.current_file_rel:
            messagebox.showinfo("No File Selected", "Select a file from the left tree first.")
            return

        # Always save what is currently in the Raw Text buffer (kept in sync with Visual Editor)
        content = self.raw_text.get("1.0", "end-1c")
        full_path = os.path.join(self.data_dir.get(), self.current_file_rel)

        try:
            os.makedirs(os.path.dirname(full_path), exist_ok=True)
            with open(full_path, "w", encoding="utf-8", newline="\n") as f:
                f.write(content)
            self.status_var.set(f"Saved {self.current_file_rel} successfully at {datetime.now().strftime('%H:%M:%S')}.")
        except Exception as e:
            messagebox.showerror("Save Error", f"Failed to save file:\n{e}")

    # -------------------------------------------------------------------------
    # Backup, Restore & Data Replacement Operations
    # -------------------------------------------------------------------------
    def refresh_backup_list(self):
        self.backup_tree.delete(*self.backup_tree.get_children())
        bdir = self.backup_dir.get()
        if not os.path.isdir(bdir):
            return

        for f in sorted(os.listdir(bdir), reverse=True):
            if f.lower().endswith(".zip"):
                full = os.path.join(bdir, f)
                stat = os.stat(full)
                dt = datetime.fromtimestamp(stat.st_mtime).strftime("%Y-%m-%d %H:%M:%S")
                sz = f"{stat.st_size / 1024:.1f} KB"
                self.backup_tree.insert("", tk.END, values=(f, dt, sz))

    def create_backup(self):
        src = self.data_dir.get()
        bdir = self.backup_dir.get()

        if not os.path.isdir(src):
            messagebox.showerror("Invalid Data Directory", f"Data directory does not exist:\n{src}")
            return

        os.makedirs(bdir, exist_ok=True)
        stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
        zip_name = f"riftfall_data_backup_{stamp}.zip"
        zip_path = os.path.join(bdir, zip_name)

        try:
            file_count = 0
            with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
                for root, dirs, files in os.walk(src):
                    for file in files:
                        full_path = os.path.join(root, file)
                        rel_path = os.path.relpath(full_path, src)
                        zf.write(full_path, rel_path)
                        file_count += 1

            self.refresh_backup_list()
            self.status_var.set(f"Created backup '{zip_name}' ({file_count} files).")
            messagebox.showinfo("Backup Complete", f"Backed up {file_count} files to:\n{zip_path}")
        except Exception as e:
            messagebox.showerror("Backup Failed", str(e))

    def restore_selected_backup(self):
        sel = self.backup_tree.selection()
        if not sel:
            messagebox.showwarning("Select Backup", "Select a backup archive from the list first.")
            return

        zip_name = self.backup_tree.item(sel[0], "values")[0]
        zip_path = os.path.join(self.backup_dir.get(), zip_name)
        target_dir = self.data_dir.get()

        confirm = messagebox.askyesno(
            "Confirm Restore & Replace",
            f"This will overwrite and replace the live data in:\n{target_dir}\n\n"
            f"with the backup archive:\n{zip_name}\n\nContinue?"
        )
        if not confirm:
            return

        self._replace_data_from_zip(zip_path, target_dir)

    def import_external_data(self):
        zip_path = filedialog.askopenfilename(
            title="Select External Data ZIP Archive to Replace Live Data",
            filetypes=[("ZIP Archives", "*.zip"), ("All Files", "*.*")]
        )
        if not zip_path:
            return

        target_dir = self.data_dir.get()
        confirm = messagebox.askyesno(
            "Safety Backup Before Replace?",
            "Would you like to create an automatic backup of your current live data before replacing it?"
        )
        if confirm:
            self.create_backup()

        self._replace_data_from_zip(zip_path, target_dir)

    def _replace_data_from_zip(self, zip_path, target_dir):
        try:
            if os.path.isdir(target_dir):
                shutil.rmtree(target_dir)
            os.makedirs(target_dir, exist_ok=True)

            with zipfile.ZipFile(zip_path, "r") as zf:
                zf.extractall(target_dir)

            self.refresh_file_tree()
            if self.current_file_rel:
                self.load_file(self.current_file_rel)
            self.status_var.set(f"Restored and replaced live data from {os.path.basename(zip_path)}")
            messagebox.showinfo("Data Replaced", f"Successfully replaced live data in:\n{target_dir}")
        except Exception as e:
            messagebox.showerror("Restore Failed", str(e))

    def delete_selected_backup(self):
        sel = self.backup_tree.selection()
        if not sel:
            return
        zip_name = self.backup_tree.item(sel[0], "values")[0]
        zip_path = os.path.join(self.backup_dir.get(), zip_name)
        if messagebox.askyesno("Delete Backup", f"Permanently delete backup '{zip_name}'?"):
            try:
                os.remove(zip_path)
                self.refresh_backup_list()
                self.status_var.set(f"Deleted backup {zip_name}")
            except Exception as e:
                messagebox.showerror("Delete Error", str(e))


if __name__ == "__main__":
    app = RiftfallDataStudio()
    app.mainloop()