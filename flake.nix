{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-24.05";
  };

  outputs = { self, nixpkgs }: 
  let
	system = "x86_64-linux";
	pkgs = nixpkgs.legacyPackages.${system};
  in {

	  packages.${system} = rec {
		  msi-ec = pkgs.callPackage ./default.nix {
			  inherit pkgs;
		  };
		  default = msi-ec;
	  };
  };
}
