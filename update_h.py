with open('include/ui_core.h', 'r') as f:
    content = f.read()

target = 'void setIrActiveRemotePath(const String& path);'
addition = '''void setIrActiveRemotePath(const String& path);
    void setIrQuickRemoteName(const String& name) { ir_quick_remote_name = name; }
    void setIrQuickButtonName(const String& name) { ir_quick_button_name = name; }'''

if target in content:
    content = content.replace(target, addition)
    with open('include/ui_core.h', 'w') as f:
        f.write(content)
    print("Updated include/ui_core.h successfully.")
else:
    print("Target not found.")
