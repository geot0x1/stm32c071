"""PyQt6 dark theme stylesheet"""

DARK_STYLESHEET = """
    QMainWindow, QWidget {
        background-color: #1a1d23;
        color: #e0e0e0;
        font-family: 'Segoe UI', sans-serif;
        font-size: 12px;
    }
    QLabel {
        color: #b0b8c8;
        background: transparent;
    }
    QComboBox {
        background-color: #252930;
        color: #e0e0e0;
        border: 1px solid #3a3f4b;
        border-radius: 5px;
        padding: 4px 8px;
        min-height: 22px;
    }
    QComboBox:hover { border-color: #5a8dee; }
    QComboBox::drop-down { border: none; }
    QComboBox QAbstractItemView {
        background-color: #252930;
        color: #e0e0e0;
        selection-background-color: #2d5da6;
        border: 1px solid #3a3f4b;
    }
    QLineEdit {
        background-color: #252930;
        color: #e0e0e0;
        border: 1px solid #3a3f4b;
        border-radius: 5px;
        padding: 4px 8px;
        min-height: 22px;
    }
    QLineEdit:hover { border-color: #5a8dee; }
    QLineEdit:focus { border-color: #5a8dee; background-color: #2a2f3a; }
    QLineEdit:disabled { background-color: #1e2128; color: #555; border-color: #2a2f3a; }
    QLineEdit::placeholder { color: #555; }
    QCheckBox {
        color: #b0b8c8;
        spacing: 6px;
    }
    QCheckBox::indicator {
        width: 14px; height: 14px;
        border: 1px solid #3a3f4b;
        border-radius: 3px;
        background-color: #252930;
    }
    QCheckBox::indicator:checked {
        background-color: #5a8dee;
        border-color: #5a8dee;
    }
    QPushButton {
        background-color: #2d3240;
        color: #c8d0e0;
        border: 1px solid #3a3f4b;
        border-radius: 5px;
        padding: 5px 14px;
        font-weight: 600;
        min-height: 24px;
    }
    QPushButton:hover { background-color: #363c4e; border-color: #5a6a88; }
    QPushButton:pressed { background-color: #252a38; }
    QPushButton:disabled { background-color: #1e2128; color: #444; border-color: #252930; }
    QTextEdit {
        background-color: #141720;
        color: #c8ffc8;
        border: 1px solid #2a2f3a;
        border-radius: 5px;
        font-family: 'Consolas', monospace;
        font-size: 11px;
        selection-background-color: #2d5da6;
    }
    QTabWidget::pane {
        border: 1px solid #2a2f3a;
        border-radius: 5px;
        background-color: #1a1d23;
    }
    QTabBar::tab {
        background-color: #1e2128;
        color: #7a8a9a;
        border: 1px solid #2a2f3a;
        border-bottom: none;
        padding: 6px 18px;
        border-top-left-radius: 5px;
        border-top-right-radius: 5px;
        margin-right: 2px;
    }
    QTabBar::tab:selected {
        background-color: #252930;
        color: #e0e0e0;
        border-color: #3a3f4b;
    }
    QTabBar::tab:hover:!selected { background-color: #22262e; color: #b0b8c8; }
    QTableWidget {
        background-color: #1a1d23;
        color: #c8d0e0;
        gridline-color: #252930;
        border: 1px solid #2a2f3a;
        border-radius: 4px;
        font-family: 'Consolas', monospace;
        font-size: 11px;
    }
    QHeaderView::section {
        background-color: #1e2128;
        color: #7a8a9a;
        border: none;
        border-right: 1px solid #2a2f3a;
        border-bottom: 1px solid #2a2f3a;
        padding: 5px 6px;
        font-family: 'Segoe UI', sans-serif;
        font-size: 11px;
        font-weight: 600;
        text-transform: uppercase;
    }
    QTableWidget::item { padding: 4px 6px; border: none; }
    QTableWidget::item:selected { background-color: #2d3a56; color: #e0e0e0; }
    QTableWidget::item:alternate { background-color: #1e2128; }
    QScrollBar:vertical {
        background: #1a1d23;
        width: 10px;
        border-radius: 5px;
    }
    QScrollBar::handle:vertical {
        background: #3a3f4b;
        border-radius: 5px;
        min-height: 20px;
    }
    QScrollBar::handle:vertical:hover { background: #5a6a88; }
    QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical { height: 0px; }
    QScrollBar:horizontal {
        background: #1a1d23;
        height: 10px;
        border-radius: 5px;
    }
    QScrollBar::handle:horizontal {
        background: #3a3f4b;
        border-radius: 5px;
    }
    QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal { width: 0px; }
    QScrollArea { border: none; background: transparent; }
    QFrame { border: none; }
    QStatusBar {
        background-color: #141720;
        color: #7a8a9a;
        border-top: 1px solid #252930;
        font-size: 11px;
    }
"""
