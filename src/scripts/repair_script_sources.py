#!/usr/bin/env python3
"""Repair damaged Commands-> calls and expand for GCC (no default args on function pointers)."""
import re
import glob
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))


def repair(text):
    # Bad merge: home radius passed to Get_Position
    text = re.sub(
        r'Commands->Get_Position\s*\(\s*([^,()]+)\s*,\s*999999(?:\.0f)?\s*\)',
        r'Commands->Get_Position(\1)',
        text,
    )
    text = re.sub(
        r'Commands->Find_Object\s*\(\s*([^,()]+)\s*,\s*NULL\s*\)',
        r'Commands->Find_Object(\1)',
        text,
    )
    text = re.sub(
        r'Commands->Get_Position\s*\(\s*Commands->Find_Object\s*\(\s*([^)]+)\s*\)\s*,\s*NULL\s*\)',
        r'Commands->Get_Position(Commands->Find_Object(\1))',
        text,
    )
    text = re.sub(
        r'Commands->Get_Position\s*\(\s*([^,()]+)\s*,\s*(\d+(?:\.\d+f)?)\s*\)',
        r'Commands->Get_Position(\1)',
        text,
    )
    text = re.sub(
        r'Commands->Get_Position\s*\(\s*([^,()]+)\s*,\s*NULL\s*\)',
        r'Commands->Get_Position(\1)',
        text,
    )
    text = re.sub(
        r'Commands->Set_Innate_Soldier_Home_Location\s*\(\s*([^,()]+)\s*,\s*Commands->Get_Position\s*\(\s*([^,()]+)\s*\)\s*\)',
        r'Commands->Set_Innate_Soldier_Home_Location(\1, Commands->Get_Position(\2), 999999.0f)',
        text,
    )

    # Extra trailing bools on Join_Conversation (max 3 after object + id)
    text = re.sub(
        r'(Commands->Join_Conversation\s*\(\s*[^;]+?),\s*(true|false)\s*,\s*(true|false)\s*,\s*(true|false)\s*,\s*(true|false)\s*,\s*(true|false)\s*\)',
        r'\1, \2, \3, \4)',
        text,
    )
    # Broken expansion: true,true,true), extra bools
    text = re.sub(
        r'Commands->Join_Conversation\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*true,\s*true,\s*true\s*\)\s*,\s*(true|false)\s*,\s*(true|false)(?:\s*,\s*(true|false))?\s*\)',
        r'Commands->Join_Conversation(\1, \2, true, true, true)',
        text,
    )
    text = re.sub(
        r'Commands->Start_Conversation\s*\(\s*([^,()]+)\s*,\s*0\s*,\s*0\s*\)',
        r'Commands->Start_Conversation(\1, 0)',
        text,
    )
    text = re.sub(
        r'Commands->Send_Custom_Event\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*0\s*,\s*0\s*\)',
        r'Commands->Send_Custom_Event(\1, \2, \3, \4, 0)',
        text,
    )
    return text


def expand_defaults(text):
    # --- Conversations ---
    text = re.sub(
        r'Commands->Join_Conversation\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Join_Conversation(\1, \2, true, true, true)',
        text,
    )
    text = re.sub(
        r'Commands->Join_Conversation\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*(true|false)\s*,\s*(true|false)\s*\)(?!\s*,)',
        r'Commands->Join_Conversation(\1, \2, \3, \4, true)',
        text,
    )
    text = re.sub(
        r'Commands->Start_Conversation\s*\(\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Start_Conversation(\1, 0)',
        text,
    )
    text = re.sub(
        r'Commands->Create_Conversation\s*\(\s*"([^"]+)"\s*\)(?!\s*,)',
        r'Commands->Create_Conversation("\1", 0, 0, true)',
        text,
    )
    text = re.sub(
        r'Commands->Create_Conversation\s*\(\s*"([^"]+)"\s*,\s*(\d+)\s*\)(?!\s*,)',
        r'Commands->Create_Conversation("\1", \2, 0, true)',
        text,
    )
    text = re.sub(
        r'Commands->Create_Conversation\s*\(\s*"([^"]+)"\s*,\s*(\d+)\s*,\s*(\d+)\s*\)(?!\s*,)',
        r'Commands->Create_Conversation("\1", \2, \3, true)',
        text,
    )
    # Create_Conversation(name, priority_expr) — exactly two arguments
    text = re.sub(
        r'Commands->Create_Conversation\s*\(\s*([^,()]+(?:\([^)]*\))?)\s*,\s*([^,)]+)\s*\)(?!\s*,)',
        r'Commands->Create_Conversation(\1, \2, 0, true)',
        text,
    )

    # Send_Custom_Event: 4 args -> add delay 0
    text = re.sub(
        r'Commands->Send_Custom_Event\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Send_Custom_Event(\1, \2, \3, \4, 0)',
        text,
    )
    text = re.sub(
        r'Commands->Send_Custom_Event\s*\(\s*([^,()]+)\s*,\s*Commands->Find_Object\s*\(\s*([^)]+)\s*\)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Send_Custom_Event(\1, Commands->Find_Object(\2), \3, \4, 0)',
        text,
    )

    text = re.sub(
        r'Commands->Apply_Damage\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*"([^"]+)"\s*\)(?!\s*,)',
        r'Commands->Apply_Damage(\1, \2, "\3", NULL)',
        text,
    )
    text = re.sub(
        r'Commands->Apply_Damage\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*Get_Parameter\s*\(\s*([^)]+)\s*\)\s*\)(?!\s*,)',
        r'Commands->Apply_Damage(\1, \2, Get_Parameter(\3), NULL)',
        text,
    )

    text = re.sub(
        r'Commands->Set_Animation\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*0\s*\)(?!\s*,)',
        r'Commands->Set_Animation(\1, \2, false, NULL, 0.0f, -1.0f, false)',
        text,
    )

    # Find_Closest_Soldier: 3 args -> add only_human true
    text = re.sub(
        r'Commands->Find_Closest_Soldier\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,)]+)\s*\)(?!\s*,)',
        r'Commands->Find_Closest_Soldier(\1, \2, \3, true)',
        text,
    )

    # Set_Animation: 6 args ending in numeric frame (not is_blended bool) -> add false
    text = re.sub(
        r'Commands->Set_Animation\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*(true|false)\s*,\s*([^,]+)\s*,\s*([^,]+)\s*,\s*([0-9.+\-]+f?)\s*\)(?!\s*,)',
        r'Commands->Set_Animation(\1, \2, \3, \4, \5, \6, false)',
        text,
    )
    # Set_Animation: 3 args (obj, name, looping) -> fill defaults
    text = re.sub(
        r'Commands->Set_Animation\s*\(\s*([^,]+)\s*,\s*([^,]+)\s*,\s*(true|false)\s*\)(?!\s*,)',
        r'Commands->Set_Animation(\1, \2, \3, NULL, 0.0f, -1.0f, false)',
        text,
    )

    text = re.sub(
        r'Commands->Trigger_Weapon\s*\(\s*([^,()]+)\s*,\s*(true|false)\s*,\s*([^,)]+)\s*\)(?!\s*,)',
        r'Commands->Trigger_Weapon(\1, \2, \3, true)',
        text,
    )

    text = re.sub(
        r'Commands->Stop_Sound\s*\(\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Stop_Sound(\1, true)',
        text,
    )

    text = re.sub(
        r'Commands->Modify_Action\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,)]+)\s*\)(?!\s*,)',
        r'Commands->Modify_Action(\1, \2, \3, true, true)',
        text,
    )

    text = re.sub(
        r'Commands->Give_PowerUp\s*\(\s*([^,()]+)\s*,\s*"([^"]+)"\s*\)(?!\s*,)',
        r'Commands->Give_PowerUp(\1, "\2", false)',
        text,
    )

    text = re.sub(
        r'Commands->Grant_Key\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Grant_Key(\1, \2, true)',
        text,
    )

    text = re.sub(
        r'Commands->Enable_Enemy_Seen\s*\(\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Enable_Enemy_Seen(\1, true)',
        text,
    )

    text = re.sub(
        r'Commands->Create_Explosion\s*\(\s*"([^"]+)"\s*,\s*(Vector3\s*\([^)]+\))\s*\)(?!\s*,)',
        r'Commands->Create_Explosion("\1", \2, NULL)',
        text,
    )
    text = re.sub(
        r'Commands->Create_Explosion\s*\(\s*"([^"]+)"\s*,\s*([^,)]+)\s*\)(?!\s*,)',
        r'Commands->Create_Explosion("\1", \2, NULL)',
        text,
    )

    text = re.sub(
        r'Commands->Get_ID\s*\(\s*([^,()]+)\s*,\s*0\s*\)',
        r'Commands->Get_ID(\1)',
        text,
    )

    text = re.sub(
        r'Commands->Create_Explosion_At_Bone\s*\(\s*"([^"]+)"\s*,\s*([^,()]+)\s*,\s*"([^"]+)"\s*\)(?!\s*,)',
        r'Commands->Create_Explosion_At_Bone("\1", \2, "\3", NULL)',
        text,
    )

    text = re.sub(
        r'Commands->Static_Anim_Phys_Goto_Frame\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Static_Anim_Phys_Goto_Frame(\1, \2, NULL)',
        text,
    )

    text = re.sub(
        r'Commands->Static_Anim_Phys_Goto_Last_Frame\s*\(\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Static_Anim_Phys_Goto_Last_Frame(\1, NULL)',
        text,
    )

    # Add_Objective: 4 args -> add NULL, 0
    text = re.sub(
        r'Commands->Add_Objective\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Add_Objective(\1, \2, \3, \4, NULL, 0)',
        text,
    )

    # Shake_Camera: 1 arg (pos only) - rare; 2 args; 3 args
    text = re.sub(
        r'Commands->Shake_Camera\s*\(\s*([^,)]+)\s*\)(?!\s*,)',
        r'Commands->Shake_Camera(\1, 25.0f, 0.25f, 1.5f)',
        text,
    )
    text = re.sub(
        r'Commands->Shake_Camera\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Shake_Camera(\1, \2, 0.25f, 1.5f)',
        text,
    )
    text = re.sub(
        r'Commands->Shake_Camera\s*\(\s*([^,()]+)\s*,\s*([^,()]+)\s*,\s*([^,()]+)\s*\)(?!\s*,)',
        r'Commands->Shake_Camera(\1, \2, \3, 1.5f)',
        text,
    )

    text = re.sub(
        r'Commands->Set_Innate_Soldier_Home_Location\s*\(\s*([^,()]+)\s*,\s*(Vector3\s*\([^)]+\))\s*\)(?!\s*,)',
        r'Commands->Set_Innate_Soldier_Home_Location(\1, \2, 999999.0f)',
        text,
    )

    text = re.sub(
        r'Commands->Set_Display_Color\s*\(\s*\)(?!\s*,)',
        r'Commands->Set_Display_Color(255, 255, 255)',
        text,
    )

    return text


def main():
    for path in glob.glob(os.path.join(SCRIPT_DIR, '*.cpp')):
        with open(path, 'r', encoding='latin-1', errors='replace') as f:
            text = f.read()
        new = expand_defaults(repair(text))
        if new != text:
            with open(path, 'w', encoding='latin-1') as f:
                f.write(new)
            print('updated', os.path.basename(path))


if __name__ == '__main__':
    main()
