/* test_swipe.mjs — 不经 server 直接驱动 exe: 注入一次滑动, 验证 --net 模式工作
 * 用法: SIM_EXE=/path/to/main.exe node test_swipe.mjs
 */
import { spawn } from 'child_process';
import { existsSync } from 'fs';

const EXE = process.env.SIM_EXE || '..\\sim\\main.exe';
if (!existsSync(EXE)) { console.error('找不到 exe, 用 SIM_EXE 指定'); process.exit(1); }
const child = spawn(EXE, ['--net'], { stdio: ['pipe', 'pipe', 'pipe'] });
let bytes = 0;
child.stdout.on('data', d => { bytes += d.length; });
child.stderr.on('data', d => process.stdout.write('[sim] ' + d));

const sleep = ms => new Promise(r => setTimeout(r, ms));
const ev = (t, x, y) => Buffer.from([t, x & 0xff, x >> 8, y & 0xff, y >> 8]);

await sleep(1500);
console.log(`idle: bytes=${bytes}`);

// 右滑: 按下 → 一串右移 → 松开
child.stdin.write(ev(1, 30, 150));
for (let i = 1; i <= 8; i++) {
  await sleep(40);
  child.stdin.write(ev(0, 30 + i * 22, 150));
}
await sleep(40);
child.stdin.write(ev(2, 206, 150));
await sleep(2000);
console.log(`after swipe-right: bytes=${bytes}`);

child.kill();
process.exit(0);
