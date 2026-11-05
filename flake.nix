{
  description = "Dev environment for Raylib C project";

  inputs = {
    nixpkgs.url = "github:NixOS/nixpkgs/26.05";
  };

  outputs = { self, nixpkgs }:
    let
      supportedSystems = [ "x86_64-linux" "aarch64-linux" ];
      forAllSystems = nixpkgs.lib.genAttrs supportedSystems;
      nixpkgsFor = forAllSystems (system: import nixpkgs { inherit system; });
    in
    {
      devShells = forAllSystems (system:
        let
          pkgs = nixpkgsFor.${system};

          runtimeLibs = with pkgs; [
            libGL
            raylib
            libX11
            libXrandr
            libXinerama
            libXcursor
            libXi
            libXext
            libXft
            libXrender
            libxcb
            libxkbcommon
            wayland
            alsa-lib
          ];
        in
        {
          default = pkgs.mkShell {
            nativeBuildInputs = with pkgs; [
              gcc
              pkg-config
              gdb
            ];

            buildInputs = runtimeLibs;

            shellHook = ''
              export LD_LIBRARY_PATH="${pkgs.lib.makeLibraryPath runtimeLibs}:$LD_LIBRARY_PATH"
            '';
          };
        });
    };
}
