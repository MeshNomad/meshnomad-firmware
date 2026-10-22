# MeshNomad: the esptool pinned in variants/meshnomad/platformio.ini ([mn_esp32_tools], 4.8.1)
# imports intelhex, which espressif32@6.11.0 does not install into PlatformIO's Python.
# Runs before the build (pre: extra script) and installs it once if it is missing.
Import("env")  # noqa: F821

try:
    import intelhex  # noqa: F401
except ImportError:
    print("MeshNomad: installing intelhex for esptool 4.8.1")
    env.Execute('"$PYTHONEXE" -m pip install --quiet "intelhex>=2.3.0"')  # noqa: F821
