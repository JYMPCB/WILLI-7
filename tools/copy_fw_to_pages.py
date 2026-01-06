Import("env")
import os
import shutil

def after_build(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    build_dir   = env.subst("$BUILD_DIR")

    fw_src = os.path.join(build_dir, "firmware.bin")
    ota_dir = os.path.join(project_dir, "docs", "ota")
    os.makedirs(ota_dir, exist_ok=True)

    fw_dst = os.path.join(ota_dir, "firmware.bin")

    if not os.path.isfile(fw_src):
        print("[OTA] firmware.bin no encontrado en:", fw_src)
        return

    shutil.copyfile(fw_src, fw_dst)
    print("[OTA] Copiado:", fw_src, "->", fw_dst)

env.AddPostAction("buildprog", after_build)
