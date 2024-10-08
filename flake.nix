{
  description = "A very basic flake";

  inputs = {
    nixpkgs.url = "github:nixos/nixpkgs/nixos-24.05";
  };

  outputs =
    { self, nixpkgs }:
    let
      system = "x86_64-linux";
      pkgs = nixpkgs.legacyPackages.${system};
    in
    {

      packages.${system} = rec {
        msi-ec = pkgs.callPackage ./default.nix { inherit pkgs; };
        msi-ec-dev = msi-ec.overrideAttrs { development = true; };
        default = msi-ec;
      };

      formatter.${system} = pkgs.nixfmt-rfc-style;
    };
}
