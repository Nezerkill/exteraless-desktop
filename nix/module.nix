self: {
  config,
  lib,
  pkgs,
  ...
}: let
  cfg = config.programs.exteraless-desktop;
in {
  options.programs.exteraless-desktop = {
    enable = lib.mkEnableOption "Exteraless Desktop";

    package = lib.mkOption {
      type = lib.types.package;
      default = self.packages.${pkgs.system}.default;
      description = "The Exteraless Desktop package to install.";
    };

    autostart = lib.mkOption {
      type = lib.types.bool;
      default = false;
      description = "Start Exteraless Desktop automatically on login.";
    };
  };

  config = lib.mkIf cfg.enable {
    environment.systemPackages = [cfg.package];

    environment.etc = lib.mkIf cfg.autostart {
      "xdg/autostart/exteraless-desktop.desktop".source =
        "${cfg.package}/share/applications/org.exteraless.desktop.desktop";
    };
  };
}
