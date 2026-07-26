// Cardputer.jsx — physical device frame around the 240×135 screen.

const { useState } = React;

function Cardputer({ children, scale = 2, ledRGB = [50, 255, 100], showLED = true }) {
  // The M5Cardputer ADV: ~93 × 56 mm, screen ~36 × 20 mm, 56-key qwerty.
  // We render a stylised, recognisable shell — not a photoreal one.
  // Frame must fit: screen (135*scale) + bezel padding + keyboard (~80px).
  const W = 540, H = 135 * scale + 110;
  const screenW = 240 * scale, screenH = 135 * scale;
  const screenL = (W - screenW) / 2;
  const screenT = 18;

  const led = `rgb(${ledRGB.join(",")})`;
  return (
    <div style={{
      width: W, height: H, position: "relative",
      background: "#1a1a1a",
      borderRadius: 14,
      boxShadow: "inset 0 0 0 2px #000, 0 16px 40px rgba(0,0,0,.55), 0 4px 8px rgba(0,0,0,.4)",
      padding: 10,
      fontFamily: "var(--fd-font-mono)",
      color: "#666",
    }}>
      {/* Screen bezel */}
      <div style={{
        position: "absolute", left: screenL - 8, top: screenT - 8,
        width: screenW + 16, height: screenH + 16,
        background: "#0a0a0a", borderRadius: 4,
        boxShadow: "inset 0 0 0 1px #2a2a2a",
      }} />
      <div style={{
        position: "absolute", left: screenL, top: screenT,
        width: screenW, height: screenH,
        overflow: "hidden", borderRadius: 1,
      }}>
        <div style={{
          width: 240, height: 135,
          transform: `scale(${scale})`, transformOrigin: "top left",
        }}>{children}</div>
      </div>

      {/* LED dot */}
      {showLED && (
        <div style={{
          position: "absolute", right: 16, top: 18, width: 7, height: 7,
          background: led, borderRadius: "50%",
          boxShadow: `0 0 6px ${led}, 0 0 12px ${led}`,
        }} />
      )}

      {/* Brand strip — bottom corner so it can't fight the page heading */}
      <div style={{
        position: "absolute", right: 16, bottom: 4,
        fontSize: 8, letterSpacing: "0.25em", color: "#3a3a3a",
      }}>M5 · CARDPUTER</div>

      {/* Keyboard grid */}
      <Keyboard top={screenT + screenH + 16} left={14} width={W - 28} />
    </div>
  );
}

function Keyboard({ top, left, width }) {
  // 4 rows × 14 cols of stylised keycaps. Doesn't replicate the layout
  // exactly — that's the firmware's job.
  const rows = [
    "1234567890-=`",
    "qwertyuiop[]\\",
    "asdfghjkl;'\u23ce",   // enter glyph at end
    "\u21e7zxcvbnm,./\u2423", // shift / space
  ];
  return (
    <div style={{ position: "absolute", left, top, width, display: "flex", flexDirection: "column", gap: 2 }}>
      {rows.map((row, ri) => (
        <div key={ri} style={{ display: "flex", gap: 2 }}>
          {row.split("").map((c, ci) => (
            <div key={ci} style={{
              flex: 1, height: 14, borderRadius: 3,
              background: "linear-gradient(180deg, #2a2a2a, #1c1c1c)",
              boxShadow: "inset 0 1px 0 rgba(255,255,255,.05), inset 0 -1px 0 rgba(0,0,0,.6), 0 1px 0 #050505",
              color: "#888", fontSize: 7, textAlign: "center", lineHeight: "14px",
              fontFamily: "var(--fd-font-mono)",
            }}>{c.toUpperCase()}</div>
          ))}
        </div>
      ))}
    </div>
  );
}

Object.assign(window, { Cardputer });
