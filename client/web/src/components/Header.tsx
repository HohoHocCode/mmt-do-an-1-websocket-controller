import {
  Wifi,
  Terminal
} from "lucide-react";

interface HeaderProps {
  title?: string;
  connectedCount?: number;
}

export default function Header({
  connectedCount = 0,
}: HeaderProps) {
  return (
    <header className="flex items-center justify-between px-6 py-4
                       border-b border-border
                       bg-background/60 backdrop-blur-xl">
      {/* LEFT */}
      <div className="flex items-center gap-4">
        <div className="w-10 h-10 rounded-xl
                        bg-primary/20 border border-primary/30
                        flex items-center justify-center">
        <Terminal className="w-5 h-5 text-primary" />
        </div>

        <div className="flex flex-col">
        <span className="text-xs tracking-widest text-muted-foreground">
            NETWORK ADMINISTRATION & SECURITY TOOL
        </span>
        <h1 className="text-2xl font-semibold">
            Remote Control Center
        </h1>
        </div>
    </div>

      {/* RIGHT */}
      <div className="flex items-center gap-3 px-4 py-2
                      rounded-full border border-border
                      bg-secondary/50 text-sm">
        <Wifi className="w-4 h-4 text-primary" />
        <span className="text-muted-foreground">
          Connected:
        </span>
        <span className="font-semibold text-foreground">
          {connectedCount}
        </span>
      </div>
    </header>
  );
}
