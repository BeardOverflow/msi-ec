{
  stdenv,
  lib,
  fetchFromGitHub,
  linuxPackages,
  kernel ? linuxPackages.kernel,
  development ? false,
  ...
}:
let
  fs = lib.fileset;
  githubSrc = fetchFromGitHub {
    owner = "BeardOverflow";
    repo = "msi-ec";
    rev = "e5820a2b415e796db9dfb204250f7410b6662ac2";
    hash = "sha256-1nwIf5OWjJpLLRUKeSOcZ1yvBGE51rUAZLmZjkt8K04=";
  };
  localSrc = fs.union [
    ./msi-ec.c
    ./ec_memory_configuration.h
  ];
  src = if development then localSrc else githubSrc;
in
stdenv.mkDerivation rec {
  pname = "msi-ec-kmods";
  version = "2024-09-18";

  inherit src;

  hardeningDisable = [ "pic" ];

  makeFlags = kernel.makeFlags ++ [ "INSTALL_MOD_PATH=${placeholder "out"}" ];

  KSRC = "${kernel.dev}/lib/modules/${kernel.modDirVersion}/build";

  nativeBuildInputs = kernel.moduleBuildDependencies;

  buildPhase = "	make -C ${KSRC} modules \"M=$(pwd -P)\"\n";

  installPhase = ''
    make -C ${KSRC} M=$(pwd) modules_install $makeFlags
  '';

  enableParallelBuilding = true;

  meta = with lib; {
    description = "Kernel modules for MSI Embedded controller";
    homepage = "https://github.com/BeardOverflow/msi-ec";
    license = licenses.gpl3Only;
    maintainers = [ ];
    platforms = platforms.linux;
  };
}
