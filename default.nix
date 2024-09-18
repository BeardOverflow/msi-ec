{ stdenv
, lib
, fetchFromGitHub
, linuxPackages
, kernel ? linuxPackages.kernel
, ...
}:
stdenv.mkDerivation rec {
	pname = "msi-ec-kmods";
	version = "2024-09-18";

	src = fetchFromGitHub {
		owner = "BeardOverflow";
		repo = "msi-ec";
		rev = "e5820a2b415e796db9dfb204250f7410b6662ac2";
		hash = "sha256-1nwIf5OWjJpLLRUKeSOcZ1yvBGE51rUAZLmZjkt8K04=";
	};

	hardeningDisable = [
		"pic"
	];

	makeFlags = kernel.makeFlags ++ [
		"INSTALL_MOD_PATH=${placeholder "out"}"
	];

	KSRC = "${kernel.dev}/lib/modules/${kernel.modDirVersion}/build";

	nativeBuildInputs = kernel.moduleBuildDependencies;

	buildPhase = ''
		make -C ${KSRC} modules "M=$(pwd -P)"
	'';

	installPhase = ''
		make -C ${KSRC} M=$(pwd) modules_install $makeFlags
		'';

	enableParallelBuilding = true;

	meta = with lib; {
		description = "Kernel modules for MSI Embedded controller";
		homepage = "https://github.com/BeardOverflow/msi-ec";
		license = licenses.gpl3Only;
		maintainers = [];
		platforms = platforms.linux;
	};
}
