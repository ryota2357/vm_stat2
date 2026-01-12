{
  description = "vm_stat2: An improved vm_stat command for macOS";

  inputs.nixpkgs.url = "github:nixos/nixpkgs?ref=nixpkgs-unstable";

  outputs =
    { self, nixpkgs }:
    let
      mk_vm_stat2 =
        pkgs:
        pkgs.stdenv.mkDerivation (finalAttrs: {
          name = "vm_stat2";
          src = pkgs.lib.cleanSource ./.;
          makeFlags = [ "CC=${pkgs.stdenv.cc.targetPrefix}cc" ];
          buildPhase = ''
            runHook preBuild
            make
            runHook postBuild
          '';
          installPhase = ''
            runHook preInstall
            mkdir -p $out/bin
            cp build/vm_stat2 $out/bin/
            runHook postInstall
          '';
          doCheck = true;
          checkPhase = ''
            runHook preCheck
            make test
            runHook postCheck
          '';
        });
    in
    {
      packages.x86_64-darwin.default = mk_vm_stat2 nixpkgs.legacyPackages.x86_64-darwin;
      packages.aarch64-darwin.default = mk_vm_stat2 nixpkgs.legacyPackages.aarch64-darwin;
    };
}
