import { spawn, ChildProcess } from "child_process";
import path from "path";
import { config } from "./config";
import { ControllerStatus } from "./types";

interface ControllerManagerOptions {
  command: string;
  args?: string;
  workingDir?: string;
  graceMs: number;
}

function parseArgs(raw?: string): string[] {
  if (!raw) return [];
  return raw
    .split(" ")
    .map((part) => part.trim())
    .filter((part) => part.length > 0);
}

export class ControllerManager {
  private child: ChildProcess | null = null;
  private state: ControllerStatus = { status: "idle" };
  private readonly command: string;
  private readonly args: string[];
  private readonly workingDir?: string;
  private readonly graceMs: number;

  constructor(opts: ControllerManagerOptions) {
    this.command = opts.command.trim();
    this.args = parseArgs(opts.args);
    this.workingDir = opts.workingDir ? path.resolve(opts.workingDir) : undefined;
    this.graceMs = Math.max(500, opts.graceMs);
  }

  getStatus(): ControllerStatus {
    return { ...this.state };
  }

  async start(): Promise<ControllerStatus> {
    if (!this.command) {
      this.state = { status: "error", message: "CONTROLLER_CMD not set" };
      return this.state;
    }
    if (this.child && !this.child.killed) {
      this.state = {
        ...this.state,
        status: "running",
        message: "Controller already running",
      };
      return this.state;
    }

    return new Promise<ControllerStatus>((resolve) => {
      try {
        const proc = spawn(this.command, this.args, {
          cwd: this.workingDir,
          stdio: "inherit",
        });
        this.child = proc;
        this.state = {
          status: "running",
          pid: proc.pid ?? undefined,
          startedAt: Date.now(),
          command: [this.command, ...this.args].join(" ").trim(),
        };

        proc.once("exit", (code, signal) => {
          this.state = {
            status: "stopped",
            pid: undefined,
            startedAt: undefined,
            stoppedAt: Date.now(),
            lastExit: { code, signal, at: Date.now() },
            command: this.state.command,
          };
          this.child = null;
        });

        resolve({ ...this.state });
      } catch (err) {
        const message = err instanceof Error ? err.message : "Failed to start controller";
        this.state = { status: "error", message };
        this.child = null;
        resolve({ ...this.state });
      }
    });
  }

  async stop(): Promise<ControllerStatus> {
    if (!this.child) {
      this.state = { status: "idle", message: "Controller not running", command: this.state.command };
      return this.state;
    }

    const proc = this.child;
    return new Promise<ControllerStatus>((resolve) => {
      let settled = false;

      const finalize = (status: ControllerStatus) => {
        if (settled) return;
        settled = true;
        this.child = null;
        this.state = status;
        resolve({ ...this.state });
      };

      const timer = setTimeout(() => {
        if (!proc.killed) {
          proc.kill("SIGKILL");
        }
      }, this.graceMs);

      proc.once("exit", (code, signal) => {
        clearTimeout(timer);
        finalize({
          status: "stopped",
          pid: undefined,
          startedAt: undefined,
          stoppedAt: Date.now(),
          lastExit: { code, signal, at: Date.now() },
          command: this.state.command,
        });
      });

      const terminated = proc.kill("SIGTERM");
      if (!terminated) {
        clearTimeout(timer);
        finalize({
          status: "error",
          message: "Failed to send SIGTERM",
          command: this.state.command,
        });
      }
    });
  }

  async restart(): Promise<ControllerStatus> {
    const stopped = await this.stop();
    if (stopped.status === "error") return stopped;
    return this.start();
  }
}

export const controllerManager = new ControllerManager({
  command: config.CONTROLLER_CMD,
  args: config.CONTROLLER_ARGS,
  workingDir: config.CONTROLLER_WORKDIR,
  graceMs: config.CONTROLLER_GRACE_MS,
});
