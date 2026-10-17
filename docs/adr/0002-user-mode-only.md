# Stay in user mode; no kernel-mode brightness or display-off

The app runs as a normal user-mode EBOOT and uses only user-mode calls (RTC read, `scePowerTick`, CPU clock). Real brightness control and display-off are kernel/driver-only and would need a kernel-mode module or CFW plugin that we have not tried on the device, so dimming and a night theme are deferred rather than faked with risky code. If dimming becomes worth it, revisit this ADR.
