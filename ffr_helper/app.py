"""Cross-platform Qt desktop interface for working-copy practice saves."""
from __future__ import annotations

import json
from pathlib import Path
import sys
import zipfile

from PySide6.QtCore import Qt, QTimer
from PySide6.QtWidgets import (QApplication, QCheckBox, QComboBox, QFileDialog, QFormLayout,
                              QGridLayout, QGroupBox, QHBoxLayout, QHeaderView, QLabel,
                              QLineEdit, QListWidget, QMainWindow, QMessageBox, QPushButton,
                              QSpinBox, QTabWidget, QTableWidget, QTableWidgetItem,
                              QVBoxLayout, QWidget, QAbstractItemView)

from . import __version__
from .editor import SaveEditor
from .progression import INT_MAX, level_at, reference_data, route_totals
from .storage import CheckpointStore, app_directory, discover_saves, write_save, atomic_write
from .bridge import FieldSaveBridge, prepare_adapter
from .runtime import RuntimeRecorder, prepare_recorder, export_report


STYLE = """QMainWindow,QWidget{background:#f4f5f3;color:#22353e;font-family:'Segoe UI';font-size:13px}
QLabel,QCheckBox{background:transparent}
QCheckBox::indicator{width:13px;height:13px;border:1px solid #9fb3b7;border-radius:2px;background:#fff}
QCheckBox::indicator:checked{background:#214f5b;border-color:#214f5b}
QCheckBox::indicator:disabled{background:#e9eeec;border-color:#c9d3d2}
QLabel#title{font-size:28px;font-weight:600;color:#173e4b}
QLabel#subtitle{color:#60727a;margin-bottom:8px}
QTabWidget::pane{border:1px solid #ccd5d5;background:#fff;border-radius:5px}
QTabBar::tab{padding:12px 18px;background:#e7edeb;border:0;margin-right:3px}
QTabBar::tab:selected{background:#fff;color:#173e4b;border-top:3px solid #b89a59}
QGroupBox{border:1px solid #d3dcdc;border-radius:5px;margin-top:14px;padding:18px 12px 12px;background:#fff}
QGroupBox::title{subcontrol-origin:margin;left:12px;padding:0 5px;color:#173e4b;font-weight:600}
QPushButton{padding:8px 13px;border:1px solid #bac9cb;background:#fff;border-radius:4px}
QPushButton:hover{background:#edf4f1;border-color:#73938f}
QPushButton:disabled{color:#9aa7aa;background:#eef0ee;border-color:#d6dcdc}
QPushButton#primary{background:#214f5b;color:white;border:1px solid #214f5b}
QPushButton#primary:hover{background:#2a6371}
QLineEdit,QComboBox,QSpinBox{background:white;padding:6px;border:1px solid #bccbcd;border-radius:3px;min-height:20px}
QComboBox QAbstractItemView{background:#fff;selection-background-color:#d6e8e4}
QTableWidget,QListWidget{background:#fff;border:1px solid #d1dada;gridline-color:#edf0ef}
QHeaderView::section{background:#eaf0ed;border:0;padding:7px;font-weight:600}
QLabel#notice{background:#edf3f0;color:#375b60;padding:12px;border-left:3px solid #b89a59}
QLabel#status{color:#556f73;padding:8px}
"""


def spin(value=0, maximum=INT_MAX):
    widget = QSpinBox()
    widget.setRange(0, maximum)
    widget.setValue(value)
    widget.setGroupSeparatorShown(True)
    return widget


def text(message, object_name=None):
    widget = QLabel(message)
    widget.setWordWrap(True)
    if object_name:
        widget.setObjectName(object_name)
    return widget


def button(label, callback, primary=False):
    widget = QPushButton(label)
    widget.clicked.connect(callback)
    if primary:
        widget.setObjectName('primary')
    return widget


class MainWindow(QMainWindow):
    def __init__(self):
        super().__init__()
        self.setWindowTitle(f'FFR Helper {__version__} — Practice & routing')
        self.resize(1120, 820)
        self.reference = reference_data()
        self.editor = None
        self.source = None
        self.dirty = False
        self.bridge = None
        self.recorder = None
        self.runtime_report = None
        self.store = CheckpointStore()
        self.settings_path = app_directory() / 'settings.json'
        try:
            self.settings = json.loads(self.settings_path.read_text(encoding='utf8'))
        except (OSError, ValueError):
            self.settings = {}
        frame = QWidget()
        layout = QVBoxLayout(frame)
        layout.setContentsMargins(24, 18, 24, 14)
        layout.addWidget(text('FFR Helper', 'title'))
        layout.addWidget(text('Practice saves · EXP & vision MRP · Inventory · Route planning', 'subtitle'))
        toolbar = QHBoxLayout()
        toolbar.addWidget(button('Open save…', self.browse_save))
        self.export_button = button('Export edited copy…', self.export_copy, True)
        self.write_button = button('Write to selected slot', self.write_slot)
        toolbar.addWidget(self.export_button)
        toolbar.addWidget(self.write_button)
        toolbar.addStretch()
        toolbar.addWidget(QLabel('Reveal through chapter'))
        self.chapter = spin(int(self.settings.get('chapter', 3)), 9)
        self.chapter.setMinimum(1)
        self.chapter.setFixedWidth(70)
        self.chapter.valueChanged.connect(self.change_chapter)
        toolbar.addWidget(self.chapter)
        layout.addLayout(toolbar)
        self.file_label = text('Open a save slot to start. All edits stay in a working copy until you export or write.', 'notice')
        layout.addWidget(self.file_label)
        self.tabs = QTabWidget()
        self.tabs.addTab(self.save_page(), 'Saves && checkpoints')
        self.tabs.addTab(self.progress_page(), 'EXP && vision MRP')
        self.tabs.addTab(self.inventory_page(), 'Items && equipment')
        self.tabs.addTab(self.route_page(), 'Route planner')
        self.tabs.addTab(self.practice_page(), 'Live practice')
        self.tabs.addTab(self.runtime_page(), 'Runtime research')
        layout.addWidget(self.tabs, 1)
        self.status = text('Offline · Originals remain untouched until you choose “Write to selected slot”.', 'status')
        layout.addWidget(self.status)
        self.setCentralWidget(frame)
        self.setStyleSheet(STYLE)
        self.refresh_slots()
        self.refresh_checkpoints()
        self.refresh()
        self.bridge_timer = QTimer(self)
        self.bridge_timer.timeout.connect(self.poll_bridge)
        self.bridge_timer.timeout.connect(self.poll_recorder)
        self.bridge_timer.start(500)

    def guard(self, operation):
        try:
            return operation()
        except (ValueError, OSError, RuntimeError, KeyError, zipfile.BadZipFile) as error:
            QMessageBox.warning(self, 'FFR Helper', str(error))
            return None

    def require_editor(self):
        if self.editor is None:
            raise ValueError('Open a game save first.')
        return self.editor

    def save_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        group = QGroupBox('Detected game saves')
        box = QVBoxLayout(group)
        self.slots = QComboBox()
        box.addWidget(self.slots)
        row = QHBoxLayout()
        row.addWidget(button('Open selected', self.open_selected))
        row.addWidget(button('Use selected as write target', self.set_target))
        row.addWidget(button('Refresh', self.refresh_slots))
        row.addWidget(button('Add save folder…', self.add_folder))
        row.addStretch()
        box.addLayout(row)
        box.addWidget(text('Windows saves are detected automatically. On Linux/Steam Deck, add the FFRS/Steam folder inside the game’s Proton prefix if it is in another library. On macOS, choose the save folder used by your game environment.'))
        layout.addWidget(group)
        group = QGroupBox('Named checkpoints')
        box = QVBoxLayout(group)
        row = QHBoxLayout()
        self.checkpoint_name = QLineEdit()
        self.checkpoint_name.setPlaceholderText('e.g. Before the next boss — route A')
        row.addWidget(self.checkpoint_name)
        row.addWidget(button('Capture working save', self.capture_checkpoint, True))
        box.addLayout(row)
        self.checkpoints = QListWidget()
        box.addWidget(self.checkpoints, 1)
        row = QHBoxLayout()
        row.addWidget(button('Open checkpoint', self.open_checkpoint))
        row.addWidget(button('Import checkpoint…', self.import_checkpoint))
        row.addWidget(button('Refresh', self.refresh_checkpoints))
        row.addStretch()
        box.addLayout(row)
        box.addWidget(text('Checkpoints contain a complete ordinary game save, including its saved position and progression. Open one here, then write it to a slot while the game is closed and load that slot in the game. They do not capture a running battle.'))
        layout.addWidget(group, 1)
        return page

    def progress_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        row = QHBoxLayout()
        self.unit = QComboBox()
        self.unit.setMinimumWidth(220)
        self.unit.currentIndexChanged.connect(self.sync_unit)
        row.addWidget(QLabel('Character'))
        row.addWidget(self.unit)
        row.addStretch()
        layout.addLayout(row)
        self.extra_forms = QCheckBox('Include additional party forms from revealed chapters')
        self.extra_forms.toggled.connect(self.refresh)
        layout.addWidget(self.extra_forms)
        grid = QHBoxLayout()
        group = QGroupBox('Character EXP')
        form = QFormLayout(group)
        self.exp_current = text('—')
        self.exp_target, self.exp_reward = spin(), spin()
        self.exp_target.setObjectName('expTarget')
        self.exp_target.valueChanged.connect(self.preview_progress)
        self.exp_reward.valueChanged.connect(self.preview_progress)
        form.addRow('Stored total EXP', self.exp_current)
        form.addRow('Set total EXP to', self.exp_target)
        form.addRow('Next battle’s credited EXP', self.exp_reward)
        self.exp_preview = text('Open a save to preview levels.')
        self.exp_preview.setObjectName('expPreview')
        form.addRow(self.exp_preview)
        form.addRow(button('Set total EXP in working save', self.apply_exp, True))
        grid.addWidget(group)
        group = QGroupBox('Vision MRP — per character')
        form = QFormLayout(group)
        self.vision = QComboBox()
        self.vision.currentIndexChanged.connect(self.sync_vision)
        self.mrp_current = text('—')
        self.mrp_target, self.mrp_reward = spin(), spin()
        self.mrp_target.setObjectName('mrpTarget')
        self.mrp_target.valueChanged.connect(self.preview_progress)
        self.mrp_reward.valueChanged.connect(self.preview_progress)
        self.mrp_preview = text('Choose an owned vision.')
        self.mrp_preview.setObjectName('mrpPreview')
        form.addRow('Owned vision', self.vision)
        form.addRow('Stored MRP for this character', self.mrp_current)
        form.addRow('Set total MRP to', self.mrp_target)
        form.addRow('Next battle’s credited MRP', self.mrp_reward)
        form.addRow(self.mrp_preview)
        form.addRow(button('Set total MRP in working save', self.apply_mrp, True))
        grid.addWidget(group)
        layout.addLayout(grid)
        layout.addWidget(text('The reward inputs are a forecast; they are not added to the saved total by these buttons. Enter rewards after EXP/MRP bonuses and participation rules. Set the pre-battle total, then let the game award the next battle normally. Vision awakening is separate from MRP and is preserved.', 'notice'))
        self.stat_preview = QTableWidget(2, 7)
        self.stat_preview.setHorizontalHeaderLabels(['HP', 'MP', 'Strength', 'Stamina', 'Magic', 'Spirit', 'Speed'])
        self.stat_preview.setVerticalHeaderLabels(['Set total', 'After battle'])
        self.stat_preview.horizontalHeader().setSectionResizeMode(QHeaderView.Stretch)
        self.stat_preview.verticalHeader().setDefaultSectionSize(26)
        self.stat_preview.setMinimumHeight(105)
        self.stat_preview.setMaximumHeight(125)
        self.stat_preview.setEditTriggers(QAbstractItemView.NoEditTriggers)
        layout.addWidget(self.stat_preview, 1)
        layout.addWidget(text('Base stats use fixed level growth; equipment, seeds and mastery bonuses are additional. Levels update when the edited save loads.'))
        return page

    def inventory_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        self.item_search = QLineEdit()
        self.item_search.setPlaceholderText('Filter inventory by item name or ID…')
        self.item_search.textChanged.connect(self.refresh_inventory)
        layout.addWidget(self.item_search)
        self.item_table = QTableWidget(0, 3)
        self.item_table.setObjectName('inventoryTable')
        self.item_table.setHorizontalHeaderLabels(['Item', 'ID', 'Quantity'])
        self.item_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.item_table.setSelectionBehavior(QAbstractItemView.SelectRows)
        self.item_table.setSelectionMode(QAbstractItemView.SingleSelection)
        self.item_table.setEditTriggers(QAbstractItemView.NoEditTriggers)
        self.item_table.itemSelectionChanged.connect(self.select_inventory_item)
        layout.addWidget(self.item_table, 1)
        row = QHBoxLayout()
        self.item_picker = QComboBox()
        self.item_picker.setEditable(True)
        self.item_picker.setMinimumWidth(330)
        self.quantity = spin(1, 999999)
        row.addWidget(self.item_picker, 1)
        row.addWidget(QLabel('Quantity'))
        row.addWidget(self.quantity)
        row.addWidget(button('Set count', self.set_item))
        row.addWidget(button('Add', self.add_item))
        row.addWidget(button('Remove', self.remove_item))
        layout.addLayout(row)
        group = QGroupBox('Equipment for the character selected in EXP & vision MRP')
        grid = QGridLayout(group)
        self.equipment = {}
        for index, (key, label) in enumerate([('weapon', 'Weapon'), ('armor', 'Armour'), ('accessory1', 'Accessory 1'), ('accessory2', 'Accessory 2')]):
            combo = QComboBox()
            self.equipment[key] = combo
            grid.addWidget(QLabel(label), 0, index)
            grid.addWidget(combo, 1, index)
        grid.addWidget(button('Apply equipment', self.apply_equipment, True), 2, 0, 1, 4)
        layout.addWidget(group)
        layout.addWidget(text('Pickers show items through your reveal limit and items already in this save. Equipment choices are limited to owned, compatible items. Changes affect the working save until exported.'))
        return page

    def route_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.addWidget(text('Plan credited rewards for the character and vision selected in EXP & vision MRP. Starting totals below can be edited independently of the loaded save.'))
        row = QHBoxLayout()
        self.route_exp, self.route_mrp = spin(), spin()
        self.route_exp.valueChanged.connect(self.calculate_route)
        self.route_mrp.valueChanged.connect(self.calculate_route)
        row.addWidget(QLabel('Starting total EXP'))
        row.addWidget(self.route_exp)
        row.addWidget(QLabel('Starting total MRP'))
        row.addWidget(self.route_mrp)
        row.addWidget(button('Use saved totals', self.route_from_save))
        layout.addLayout(row)
        self.route_table = QTableWidget(0, 4)
        self.route_table.setHorizontalHeaderLabels(['Encounter / note', 'Repeats', 'EXP per repeat', 'MRP per repeat'])
        self.route_table.horizontalHeader().setSectionResizeMode(0, QHeaderView.Stretch)
        self.route_table.itemChanged.connect(self.calculate_route)
        layout.addWidget(self.route_table, 1)
        row = QHBoxLayout()
        row.addWidget(button('Add encounter', self.add_route_row))
        row.addWidget(button('Remove selected', self.remove_route_row))
        row.addWidget(button('Load route…', self.load_route))
        row.addWidget(button('Save route…', self.save_route))
        row.addStretch()
        layout.addLayout(row)
        self.route_result = text('Add encounters to calculate planned totals.', 'notice')
        self.route_result.setObjectName('routeResult')
        layout.addWidget(self.route_result)
        layout.addWidget(button('Apply planned EXP/MRP totals to working save', self.apply_route, True))
        layout.addWidget(text('Use totals at the point just before your next test battle. This planner sums the rewards you enter; it does not assume that every party member or vision receives the same reward.'))
        return page

    def practice_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        group = QGroupBox('Player counters')
        form = QFormLayout(group)
        self.counters = {}
        for key, label in [('currentMoney', 'Gil'), ('EscapeNum', 'Successful escapes'), ('WinNum', 'Battles won'), ('StealSuccessNum', 'Successful steals')]:
            self.counters[key] = spin()
            form.addRow(label, self.counters[key])
        form.addRow(button('Apply counters to working save', self.apply_counters))
        layout.addWidget(group)
        group = QGroupBox('Running-game features')
        box = QVBoxLayout(group)
        self.save_anywhere = QCheckBox('Enable saving anywhere while exploring (experimental adapter)')
        self.save_anywhere.setEnabled(False)
        self.save_anywhere.toggled.connect(self.toggle_field_save)
        box.addWidget(self.save_anywhere)
        row = QHBoxLayout()
        row.addWidget(button('Prepare field-save adapter…', self.prepare_bridge))
        row.addWidget(button('Connect adapter folder…', self.connect_bridge))
        row.addStretch()
        box.addLayout(row)
        self.restart_battle = QPushButton('Battle-opening restart — research pending')
        self.restart_battle.setEnabled(False)
        box.addWidget(self.restart_battle)
        self.bridge_status = text('Offline. The optional field-save adapter requires a compatible UE4SS installation and still needs live game testing. It changes the save-point check while leaving other native restrictions in place. Battle-opening restart is not implemented in this version; an ordinary save does not contain complete battle state.', 'notice')
        box.addWidget(self.bridge_status)
        layout.addWidget(group)
        layout.addStretch()
        return page

    def prepare_bridge(self):
        folder = QFileDialog.getExistingDirectory(self, 'Choose an existing compatible UE4SS Mods directory')
        if folder:
            def operation():
                target = prepare_adapter(Path(folder))
                self.attach_bridge(target)
                self.status.setText('Optional adapter prepared. Start the game with a compatible UE4SS loader; no game binaries were changed.')
            self.guard(operation)

    def runtime_page(self):
        page = QWidget()
        layout = QVBoxLayout(page)
        layout.addWidget(text('Native battle restart and save states are under development. This read-only recorder collects encounter setup and available combat values for compatibility research. It does not restore state or change gameplay.', 'notice'))
        group = QGroupBox('Optional runtime recorder — read only')
        box = QVBoxLayout(group)
        row = QHBoxLayout()
        row.addWidget(button('Prepare recorder…', self.prepare_runtime))
        row.addWidget(button('Connect recorder folder…', self.connect_runtime))
        self.runtime_disconnect = button('Disconnect', self.disconnect_runtime)
        self.runtime_disconnect.setEnabled(False)
        row.addWidget(self.runtime_disconnect)
        row.addStretch()
        box.addLayout(row)
        box.addWidget(text('Requires a compatible UE4SS installation. It is separate from the “Saving anywhere” adapter, whose development and testing are deferred. Installing or connecting the recorder never enables field saving.'))
        self.runtime_status = text('Not connected. No game files or running game state have been changed.')
        box.addWidget(self.runtime_status)
        row = QHBoxLayout()
        self.runtime_capture = button('Capture diagnostic report', self.capture_runtime)
        self.runtime_export = button('Export report…', self.export_runtime)
        self.runtime_capture.setEnabled(False)
        self.runtime_export.setEnabled(False)
        row.addWidget(self.runtime_capture)
        row.addWidget(self.runtime_export)
        row.addStretch()
        box.addLayout(row)
        layout.addWidget(group)
        group = QGroupBox('What a reliable restore still needs')
        box = QVBoxLayout(group)
        box.addWidget(text('A battle-opening restart must restore party resources, items, progression and encounter setup, then restart the battle without duplicating rewards or story events.'))
        box.addWidget(text('A mid-battle save state additionally needs enemy AI, turn order, statuses, summons, pending actions, event state and RNG. The normal save does not contain this state, and copying reflected values alone is incomplete.'))
        box.addWidget(text('Diagnostic reports are labelled non-restorable and cannot be opened as game saves or checkpoints. Detailed reports contain numeric IDs from the current encounter; they are not displayed automatically.'))
        layout.addWidget(group)
        layout.addStretch()
        return page

    def prepare_runtime(self):
        folder = QFileDialog.getExistingDirectory(self, 'Choose an existing UE4SS Mods directory for the read-only recorder')
        if folder:
            self.guard(lambda: self.attach_runtime(prepare_recorder(Path(folder))))

    def connect_runtime(self):
        folder = QFileDialog.getExistingDirectory(self, 'Choose the FFRHelperRuntime recorder directory', self.settings.get('recorderFolder', ''))
        if folder:
            self.guard(lambda: self.attach_runtime(Path(folder)))

    def attach_runtime(self, folder):
        recorder = RuntimeRecorder(folder)
        recorder.heartbeat()
        self.disconnect_runtime()
        self.recorder = recorder
        self.settings['recorderFolder'] = str(folder)
        self.save_settings()
        self.runtime_disconnect.setEnabled(True)
        self.runtime_status.setText('Waiting for the read-only recorder. This does not enable “Saving anywhere”.')

    def disconnect_runtime(self):
        if self.recorder is not None:
            self.guard(self.recorder.disconnect)
        self.recorder = None
        self.runtime_report = None
        self.runtime_capture.setEnabled(False)
        self.runtime_export.setEnabled(False)
        self.runtime_disconnect.setEnabled(False)
        self.runtime_status.setText('Disconnected. Capture requests and observation expire automatically.')

    def capture_runtime(self):
        if self.recorder is not None:
            def operation():
                self.recorder.capture()
                self.runtime_report = None
                self.runtime_export.setEnabled(False)
                self.runtime_status.setText('Observation requested. The game continues normally; nothing is restored or changed.')
            self.guard(operation)

    def poll_recorder(self):
        if self.recorder is None:
            return
        try:
            self.recorder.heartbeat()
            state = self.recorder.status()
            self.runtime_capture.setEnabled(state is not None)
            if state is None:
                self.runtime_status.setText('Recorder not available. Waiting for a fresh matching connection.')
                return
            report = self.recorder.report()
            if report is not None:
                self.runtime_report = report
                self.runtime_export.setEnabled(True)
                self.runtime_status.setText(f'Diagnostic report captured · {len(report["events"])} recorded encounter events. Incomplete coverage; this is not a save state.')
            elif self.runtime_report is None:
                self.runtime_status.setText(state['message'])
        except (OSError, ValueError):
            self.runtime_capture.setEnabled(False)
            self.runtime_status.setText('Recorder communication failed. No game state was changed.')

    def export_runtime(self):
        if self.runtime_report is None:
            return
        name, _ = QFileDialog.getSaveFileName(self, 'Export a non-restorable runtime report', 'observation.ffrruntime.json', 'Runtime diagnostic report (*.ffrruntime.json)')
        if name:
            self.guard(lambda: export_report(Path(name), self.runtime_report))

    def connect_bridge(self):
        folder = QFileDialog.getExistingDirectory(self, 'Choose the installed FFRHelper adapter directory', self.settings.get('adapterFolder', ''))
        if folder:
            self.guard(lambda: self.attach_bridge(Path(folder)))

    def attach_bridge(self, folder):
        bridge = FieldSaveBridge(folder)
        bridge.heartbeat()
        if self.bridge is not None:
            self.bridge.stop()
        self.bridge = bridge
        self.settings['adapterFolder'] = str(folder)
        self.save_settings()
        self.bridge_status.setText('Waiting for the optional adapter’s heartbeat. Field saving starts disabled on every connection.')

    def toggle_field_save(self, checked):
        if self.bridge is not None:
            self.bridge.requested = checked
            self.guard(self.bridge.heartbeat)

    def poll_bridge(self):
        if self.bridge is None:
            return
        try:
            self.bridge.heartbeat()
            state = self.bridge.status()
            self.save_anywhere.setEnabled(bool(state and state['ready']))
            if not state or not state['ready']:
                self.bridge.requested = False
                self.save_anywhere.blockSignals(True)
                self.save_anywhere.setChecked(False)
                self.save_anywhere.blockSignals(False)
                self.bridge_status.setText('Adapter is unavailable or not ready. Field-save override is off. ' + (state.get('message', '') if state else 'Start the game with the optional adapter installed.'))
            else:
                self.bridge_status.setText('Experimental adapter connected · Field saving ' + ('ON' if state['enabled'] else 'off') + '. ' + state.get('message', '') + '. Battle-opening restart remains unavailable in this version.')
        except (OSError, ValueError):
            self.bridge.requested = False
            self.save_anywhere.setEnabled(False)
            self.bridge_status.setText('Adapter connection failed. The override expires automatically.')

    def refresh_slots(self):
        self.slots.clear()
        for path in discover_saves(self.settings.get('saveFolders', [])):
            self.slots.addItem(f'{path.name} — {path.parent.parent.name}', str(path))
        if not self.slots.count():
            self.slots.addItem('No slots detected — open a file or add its folder', '')

    def add_folder(self):
        folder = QFileDialog.getExistingDirectory(self, 'Choose FFRS/Steam or your account’s save folder')
        if folder:
            self.settings.setdefault('saveFolders', []).append(folder)
            self.save_settings()
            self.refresh_slots()

    def save_settings(self):
        self.settings['chapter'] = self.chapter.value()
        self.guard(lambda: atomic_write(self.settings_path, json.dumps(self.settings).encode()))

    def change_chapter(self):
        self.save_settings()
        if hasattr(self, 'status'):
            self.refresh()

    def confirm_discard(self):
        return not self.dirty or QMessageBox.question(self, 'Discard working edits?', 'Open another save and discard the current unexported edits?') == QMessageBox.Yes

    def browse_save(self):
        name, _ = QFileDialog.getOpenFileName(self, 'Open a GameSaveData slot', '', 'Game save (*.sav);;All files (*)')
        if name:
            self.open_path(Path(name))

    def open_selected(self):
        if self.slots.currentData():
            self.open_path(Path(self.slots.currentData()))

    def set_target(self):
        if self.slots.currentData() and self.editor is not None:
            self.source = Path(self.slots.currentData())
            self.refresh()
            self.status.setText('Write target selected. The working checkpoint has not been replaced or written.')

    def open_path(self, path):
        if not self.confirm_discard():
            return
        def operation():
            editor = SaveEditor.open(path.read_bytes())
            self.editor, self.source, self.dirty = editor, path, False
            self.refresh()
            self.status.setText('Opened and verified · Edits are held in memory; the source slot is unchanged.')
        self.guard(operation)

    def refresh(self):
        loaded = self.editor is not None
        self.export_button.setEnabled(loaded)
        self.write_button.setEnabled(loaded and self.source is not None)
        if not loaded:
            return
        self.file_label.setText(f'Target: {self.source.name if self.source else "Choose a slot in Saves & checkpoints"} · Working copy' + (' · Unsaved edits' if self.dirty else ' · No edits'))
        previous = self.unit.currentData()
        self.unit.blockSignals(True)
        self.unit.clear()
        for uid in self.editor.units():
            character = self.reference['characters'].get(str(uid))
            if character and character['chapter'] <= self.chapter.value() and (not character['additionalForm'] or self.extra_forms.isChecked()):
                self.unit.addItem(character['name'], uid)
        index = self.unit.findData(previous)
        self.unit.setCurrentIndex(index if index >= 0 else 0)
        self.unit.blockSignals(False)
        previous = self.vision.currentData()
        self.vision.blockSignals(True)
        self.vision.clear()
        for row in self.editor.root['visionList'].structs():
            vid = row['ID'].integer()
            data = self.reference['visions'].get(str(vid))
            if data:
                self.vision.addItem(data['name'], vid)
        index = self.vision.findData(previous)
        self.vision.setCurrentIndex(index if index >= 0 else 0)
        self.vision.blockSignals(False)
        previous_item = self.item_picker.currentData()
        self.item_picker.clear()
        for iid, item in sorted(self.reference['items'].items(), key=lambda pair: pair[1]['name']):
            if item['chapter'] <= self.chapter.value() or int(iid) in self.editor.inventory():
                self.item_picker.addItem(f'{item["name"]}  ·  {iid}', int(iid))
        previous_index = self.item_picker.findData(previous_item)
        if previous_index >= 0:
            self.item_picker.setCurrentIndex(previous_index)
        player = self.editor.root['Player'].struct()
        for key, widget in self.counters.items():
            widget.setValue(player[key].integer())
        self.sync_unit()
        self.refresh_inventory()

    def sync_unit(self):
        if self.editor is None or self.unit.currentData() is None:
            return
        current = self.editor.experience(self.unit.currentData())
        self.exp_current.setText(f'{current:,}')
        self.exp_target.setValue(current)
        self.sync_vision()
        self.refresh_equipment()
        self.preview_progress()
        self.calculate_route()

    def sync_vision(self):
        if self.editor is None or self.unit.currentData() is None or self.vision.currentData() is None:
            return
        points = self.editor.vision_points(self.vision.currentData()).get(self.unit.currentData(), 0)
        self.mrp_current.setText(f'{points:,}')
        self.mrp_target.setValue(points)
        self.preview_progress()
        self.calculate_route()

    def preview_progress(self):
        uid = self.unit.currentData()
        if uid is None:
            return
        curve = self.reference['characters'][str(uid)]['levels']
        current = level_at(self.exp_target.value(), curve)
        after = level_at(min(INT_MAX, self.exp_target.value() + self.exp_reward.value()), curve)
        remaining = f'{current["remaining"]:,} EXP to the next level' if current['nextTotal'] else 'Maximum level'
        self.exp_preview.setText(f'Level {current["level"]} → {after["level"]} after the next battle.\n{remaining}.')
        keys = [('hp', 'HP'), ('mp', 'MP'), ('str', 'Strength'), ('sta', 'Stamina'), ('int', 'Magic'), ('spr', 'Spirit'), ('agi', 'Speed')]
        for c, (key, label) in enumerate(keys):
            self.stat_preview.setItem(0, c, QTableWidgetItem(str(current['stats'][key])))
            self.stat_preview.setItem(1, c, QTableWidgetItem(str(after['stats'][key])))
        vid = self.vision.currentData()
        if vid is not None:
            curve = self.reference['visions'][str(vid)]['mrp']
            current = level_at(self.mrp_target.value(), curve)
            after = level_at(min(INT_MAX, self.mrp_target.value() + self.mrp_reward.value()), curve)
            remaining = f'{current["remaining"]:,} MRP to the next tier' if current['nextTotal'] else 'Maximum tier'
            self.mrp_preview.setText(f'Mastery tier {current["level"]} → {after["level"]} after the next battle.\n{remaining}.')

    def changed(self, message):
        self.dirty = True
        self.refresh()
        self.status.setText(message + ' · Working copy only.')

    def apply_exp(self):
        def operation():
            self.require_editor().set_experience(self.unit.currentData(), self.exp_target.value())
            self.changed('Total EXP updated; permanent bonuses preserved')
        self.guard(operation)

    def apply_mrp(self):
        def operation():
            self.require_editor().set_mrp(self.vision.currentData(), self.unit.currentData(), self.mrp_target.value())
            self.changed('Total MRP updated for the selected vision/character pair')
        self.guard(operation)

    def refresh_inventory(self):
        if self.editor is None:
            return
        query = self.item_search.text().casefold()
        rows = [(iid, count, self.reference['items'].get(str(iid), {}).get('name', f'Unknown item {iid}'))
                for iid, count in self.editor.inventory().items()]
        rows = sorted((r for r in rows if query in f'{r[0]} {r[2]}'.casefold()), key=lambda r: r[2])
        self.item_table.setRowCount(len(rows))
        for row, (iid, count, name) in enumerate(rows):
            for col, value in enumerate([name, iid, count]):
                self.item_table.setItem(row, col, QTableWidgetItem(str(value)))

    def select_inventory_item(self):
        row = self.item_table.currentRow()
        if row >= 0 and self.item_table.item(row, 1):
            iid = int(self.item_table.item(row, 1).text())
            self.item_picker.setCurrentIndex(self.item_picker.findData(iid))
            self.quantity.setValue(int(self.item_table.item(row, 2).text()))

    def edit_item(self, direction):
        def operation():
            editor = self.require_editor()
            iid = self.item_picker.currentData()
            if iid is None or self.item_picker.currentText() != self.item_picker.itemText(self.item_picker.currentIndex()):
                raise ValueError('Choose an item from the picker.')
            count = self.quantity.value()
            if direction:
                count = max(0, editor.inventory().get(iid, 0) + direction * count)
            editor.set_item(iid, count)
            self.changed('Inventory quantity updated')
        self.guard(operation)

    def set_item(self): self.edit_item(0)
    def add_item(self): self.edit_item(1)
    def remove_item(self): self.edit_item(-1)

    def refresh_equipment(self):
        uid = self.unit.currentData()
        if uid is None or self.editor is None:
            return
        character = self.reference['characters'][str(uid)]
        row = self.editor.units()[uid]
        current = dict(weapon=row['EquipWeaponId'].integer(), armor=row['EquipArmorId'].integer())
        accessories = row['EquipAccessoryIdList'].integers()
        current.update(accessory1=accessories[0], accessory2=accessories[1])
        for slot, combo in self.equipment.items():
            combo.clear()
            combo.addItem('None', 0)
            expected = 1 if slot == 'weapon' else 2 if slot == 'armor' else 3
            for iid, count in self.editor.inventory().items():
                data = self.reference['items'].get(str(iid), {})
                gear = data.get('gear')
                if not count or not gear or gear['type'] != expected or (gear['sex'] and gear['sex'] != character['sex']): continue
                if expected == 1 and gear['detail'] != character['weaponType']: continue
                if expected == 2 and gear['detail'] not in character['armorTypes']: continue
                combo.addItem(data['name'], iid)
            index = combo.findData(current[slot])
            combo.setCurrentIndex(index if index >= 0 else 0)

    def apply_equipment(self):
        def operation():
            editor = self.require_editor()
            clone = SaveEditor.open(editor.export())
            for slot, combo in self.equipment.items():
                clone.set_equipment(self.unit.currentData(), slot, combo.currentData(), self.reference)
            self.editor = clone
            self.changed('Equipment updated')
        self.guard(operation)

    def apply_counters(self):
        def operation():
            for key, widget in self.counters.items():
                self.require_editor().set_counter(key, widget.value())
            self.changed('Player counters updated')
        self.guard(operation)

    def export_copy(self):
        if self.editor is None:
            return
        name, _ = QFileDialog.getSaveFileName(self, 'Export an edited practice save', str(app_directory() / 'exports/GameSaveData10.sav'), 'Game save (*.sav)')
        if name:
            def operation():
                destination = Path(name)
                same = self.source and destination.resolve() == self.source.resolve()
                if same:
                    raise ValueError('Use “Write to selected slot” to replace the opened game slot with a backup.')
                game_folders = {p.parent for p in discover_saves(self.settings.get('saveFolders', []))}
                if self.source is not None:
                    game_folders.add(self.source.parent.resolve())
                write_save(destination, self.editor.export(), self.store,
                           live_slot=destination.parent.resolve() in game_folders)
                self.status.setText('Edited copy exported and verified. The source slot is unchanged.')
                self.refresh_checkpoints()
            self.guard(operation)

    def write_slot(self):
        if self.editor is None or self.source is None:
            return
        if QMessageBox.question(self, 'Write practice save?', f'Replace {self.source.name} with the working copy? A verified checkpoint of the current file will be created first. Close the game before continuing. Steam Cloud may later sync this practice save.') != QMessageBox.Yes:
            return
        def operation():
            backup = write_save(self.source, self.editor.export(), self.store, live_slot=True)
            self.dirty = False
            self.refresh()
            self.refresh_checkpoints()
            self.status.setText('Slot written and verified; original backed up. Load this slot in the game to apply it.')
        self.guard(operation)

    def capture_checkpoint(self):
        def operation():
            editor = self.require_editor()
            name = self.checkpoint_name.text().strip() or 'Practice checkpoint'
            self.store.capture(name, editor.export(), self.source.name if self.source else '')
            self.refresh_checkpoints()
            self.status.setText('Named checkpoint captured and verified.')
        self.guard(operation)

    def refresh_checkpoints(self):
        self.checkpoints.clear()
        for path in self.store.list():
            self.checkpoints.addItem(path.name)
            self.checkpoints.item(self.checkpoints.count()-1).setData(Qt.UserRole, str(path))

    def load_checkpoint(self, path):
        if not self.confirm_discard():
            return
        def operation():
            metadata, raw = self.store.read(path)
            self.editor = SaveEditor.open(raw)
            self.dirty = True
            self.refresh()
            self.status.setText(f'Checkpoint “{metadata["name"]}” opened. Choose a target slot before writing it.')
        self.guard(operation)

    def open_checkpoint(self):
        item = self.checkpoints.currentItem()
        if item:
            self.load_checkpoint(Path(item.data(Qt.UserRole)))

    def import_checkpoint(self):
        name, _ = QFileDialog.getOpenFileName(self, 'Open practice checkpoint', '', 'FFR checkpoint (*.ffrsnapshot)')
        if name:
            self.load_checkpoint(Path(name))

    def route_rows(self):
        rows = []
        for row in range(self.route_table.rowCount()):
            values = [self.route_table.item(row, c).text() if self.route_table.item(row, c) else '' for c in range(4)]
            rows.append(dict(name=values[0], count=int(values[1] or 0), exp=int(values[2] or 0), mrp=int(values[3] or 0)))
        return rows

    def add_route_row(self):
        self.route_table.blockSignals(True)
        row = self.route_table.rowCount()
        self.route_table.insertRow(row)
        for col, value in enumerate(['Encounter', '1', '0', '0']):
            self.route_table.setItem(row, col, QTableWidgetItem(value))
        self.route_table.blockSignals(False)
        self.calculate_route()

    def remove_route_row(self):
        if self.route_table.currentRow() >= 0:
            self.route_table.removeRow(self.route_table.currentRow())
            self.calculate_route()

    def route_from_save(self):
        if self.editor is not None and self.unit.currentData() is not None:
            self.route_exp.setValue(self.editor.experience(self.unit.currentData()))
            if self.vision.currentData() is not None:
                self.route_mrp.setValue(self.editor.vision_points(self.vision.currentData()).get(self.unit.currentData(), 0))

    def calculate_route(self):
        if not hasattr(self, 'route_result'):
            return
        try:
            exp, mrp = route_totals(self.route_exp.value(), self.route_mrp.value(), self.route_rows())
            message = f'Planned total EXP: {exp:,}     ·     Planned total MRP: {mrp:,}'
            if self.unit.currentData() is not None:
                level = level_at(exp, self.reference['characters'][str(self.unit.currentData())]['levels'])['level']
                message += f'     ·     Character level: {level}'
            if self.vision.currentData() is not None:
                tier = level_at(mrp, self.reference['visions'][str(self.vision.currentData())]['mrp'])['level']
                message += f'     ·     Vision mastery tier: {tier}'
            self.route_result.setText(message)
        except ValueError as error:
            self.route_result.setText('Check route values: ' + str(error))

    def apply_route(self):
        def operation():
            editor = self.require_editor()
            exp, mrp = route_totals(self.route_exp.value(), self.route_mrp.value(), self.route_rows())
            clone = SaveEditor.open(editor.export())
            clone.set_experience(self.unit.currentData(), exp)
            if self.vision.currentData() is not None:
                clone.set_mrp(self.vision.currentData(), self.unit.currentData(), mrp)
            self.editor = clone
            self.changed('Planned EXP/MRP totals applied to the selected character and vision')
        self.guard(operation)

    def save_route(self):
        name, _ = QFileDialog.getSaveFileName(self, 'Save route plan', 'route.ffrroute', 'FFR route (*.ffrroute)')
        if name:
            def operation():
                route = dict(format='ffr-helper-route', version=1, startEXP=self.route_exp.value(), startMRP=self.route_mrp.value(), encounters=self.route_rows())
                route_totals(route['startEXP'], route['startMRP'], route['encounters'])
                atomic_write(Path(name), json.dumps(route, indent=2).encode())
            self.guard(operation)

    def load_route(self):
        name, _ = QFileDialog.getOpenFileName(self, 'Open route plan', '', 'FFR route (*.ffrroute)')
        if name:
            def operation():
                route = json.loads(Path(name).read_text(encoding='utf8'))
                if route.get('format') != 'ffr-helper-route' or route.get('version') != 1:
                    raise ValueError('Unsupported route plan.')
                route_totals(route['startEXP'], route['startMRP'], route['encounters'])
                self.route_table.blockSignals(True)
                self.route_table.setRowCount(0)
                for row, entry in enumerate(route['encounters']):
                    self.route_table.insertRow(row)
                    for col, key in enumerate(['name', 'count', 'exp', 'mrp']):
                        self.route_table.setItem(row, col, QTableWidgetItem(str(entry.get(key, ''))))
                self.route_table.blockSignals(False)
                self.route_exp.setValue(route['startEXP'])
                self.route_mrp.setValue(route['startMRP'])
                self.calculate_route()
            self.guard(operation)

    def closeEvent(self, event):
        if self.confirm_discard():
            if self.recorder is not None:
                try:
                    self.recorder.disconnect()
                except OSError:
                    pass
            if self.bridge is not None:
                try:
                    self.bridge.stop()
                except OSError:
                    pass
            event.accept()
        else:
            event.ignore()


def main():
    app = QApplication(sys.argv)
    app.setApplicationName('FFR Helper')
    app.setOrganizationName('FFR Helper')
    try:
        window = MainWindow()
    except Exception:
        if '--self-test' in sys.argv:
            raise SystemExit(1)
        raise
    if '--self-test' in sys.argv:
        assert len(window.reference['characters']) == 8
        assert len(window.reference['items']) > 500
        window.close()
        raise SystemExit(0)
    if len(sys.argv) > 1:
        window.open_path(Path(sys.argv[1]))
    window.show()
    raise SystemExit(app.exec())


if __name__ == '__main__':
    main()
