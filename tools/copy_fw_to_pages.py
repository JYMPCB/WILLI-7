Import("env")
import os, shutil, json, datetime

def _get_define(env, name, default=None):
    # Busca -DNAME=VALUE dentro de BUILD_FLAGS
    flags = env.get("BUILD_FLAGS", [])
    for f in flags:
        s = str(f)
        if s.startswith(f"-D{name}="):
            return s.split("=", 1)[1]
    return default

def after_build(source, target, env):
    project_dir = env.subst("$PROJECT_DIR")
    build_dir   = env.subst("$BUILD_DIR")

    ota_dir = os.path.join(project_dir, "docs", "ota")
    os.makedirs(ota_dir, exist_ok=True)

    # ---- Copiar firmware.bin ----
    fw_src = os.path.join(build_dir, "firmware.bin")
    fw_dst = os.path.join(ota_dir, "firmware.bin")

    if not os.path.isfile(fw_src):
        print("[OTA] ERROR: firmware.bin no encontrado en:", fw_src)
        return

    shutil.copyfile(fw_src, fw_dst)
    print("[OTA] Copiado:", fw_src, "->", fw_dst)

    # ---- Generar latest.json ----
    fw_ver_raw = _get_define(env, "FW_VERSION", '"0.0.0"')
    fw_build_raw = _get_define(env, "FW_BUILD", str(int(datetime.datetime.now().strftime("%Y%m%d"))))

    # FW_VERSION viene como \"1.0.2\" (con comillas escapadas)
    fw_ver = fw_ver_raw.strip().strip('\\"').strip('"')
    fw_build = int(str(fw_build_raw).strip().strip('"'))

    # IMPORTANTÍSIMO: poné tu URL real aquí
    base_url = "https://jympcb.github.io/WILLI-7/ota"
    manifest = {
        "version": fw_ver,
        "build": fw_build,
        # cache-busting para evitar que baje el bin viejo
        "bin_url": f"{base_url}/firmware.bin?b={fw_build}",
        "notes": "Update disponible"
    }

    latest_path = os.path.join(ota_dir, "latest.json")
    with open(latest_path, "w", encoding="utf-8") as f:
        json.dump(manifest, f, indent=2)

    print("[OTA] Escrito:", latest_path, "version=", fw_ver, "build=", fw_build)

env.AddPostAction("buildprog", after_build)
