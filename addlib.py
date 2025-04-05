from os.path import join

Import("env")
platform = env.PioPlatform()

FRAMEWORK_DIR = platform.get_package_dir("framework-picosdk")

# default compontents
additional_rp2_components = [
    ("hardware_uart", "+<*>"),
]

for component, src_filter in additional_rp2_components:
    env.BuildSources(
        join("$BUILD_DIR", "PicoSDK%s" % component),
        join(FRAMEWORK_DIR, "src", "rp2_common", component),
        src_filter
    )