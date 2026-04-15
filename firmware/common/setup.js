// PlantHub Setup Overlay
// Injected into ESPHome web UI via js_include
// Shows claim code prominently during device provisioning
(function () {
  var state = '', code = '', overlay = null;

  function el(tag, styles, text) {
    var e = document.createElement(tag);
    if (styles) e.style.cssText = styles;
    if (text) e.textContent = text;
    return e;
  }

  function render() {
    if (overlay) { overlay.remove(); overlay = null; }
    // Do not show overlay if provisioned or if device still needs WiFi credentials.
    // This allows the ESPHome native captive portal (with network dropdown) to be visible!
    if (state === 'PROVISIONED' || state === '' || state === 'NEEDS_WIFI' || state === 'INIT') return;

    overlay = el('div', 'position:fixed;inset:0;background:linear-gradient(135deg,#f8f6f3 0%,#f0fdfa 50%,#f8f6f3 100%);z-index:99999;display:flex;align-items:center;justify-content:center;font-family:\'IBM Plex Sans\',-apple-system,sans-serif');
    // Gradient mesh overlay for visual depth
    var meshBg = el('div', 'position:absolute;inset:0;background:radial-gradient(ellipse at 20% 50%,rgba(13,148,136,0.08) 0%,transparent 50%),radial-gradient(ellipse at 80% 20%,rgba(194,113,79,0.05) 0%,transparent 50%),radial-gradient(ellipse at 50% 80%,rgba(20,184,166,0.06) 0%,transparent 50%);pointer-events:none');
    overlay.appendChild(meshBg);

    var wrap = el('div', 'text-align:center;max-width:420px;width:100%;padding:20px;position:relative;z-index:1;animation:phScaleIn 0.5s cubic-bezier(0.4,0,0.2,1)');

    // Logo with teal colors
    var logo = el('div', 'font-size:28px;font-weight:700;color:#0d9488;margin-bottom:20px;font-family:\'DM Sans\',sans-serif');
    logo.appendChild(document.createTextNode('Plant'));
    var span = el('span', 'color:#14b8a6', 'Hub');
    logo.appendChild(span);
    wrap.appendChild(logo);

    // Glassmorphism card
    var card = el('div', 'background:rgba(255,255,255,0.7);backdrop-filter:blur(12px);-webkit-backdrop-filter:blur(12px);border-radius:1rem;padding:28px;border:1px solid rgba(255,255,255,0.5);box-shadow:0 10px 15px rgba(13,148,136,0.08),0 4px 6px rgba(28,25,23,0.04)');

    if (state === 'WAITING_CLAIM' && code) {
      // Claim code badge
      card.appendChild(el('span', 'display:inline-block;padding:4px 14px;border-radius:9999px;font-size:12px;font-weight:600;background:rgba(13,148,136,0.1);color:#0d9488;margin-bottom:16px', 'Ready to Claim'));
      card.appendChild(el('p', 'margin:16px 0 4px;font-size:14px;color:#78716c;font-family:\'IBM Plex Sans\',sans-serif', 'Your claim code:'));
      // Code display with teal accent
      card.appendChild(el('div', 'font-size:36px;font-weight:700;letter-spacing:8px;font-family:\'IBM Plex Mono\',monospace;color:#0d9488;padding:18px;background:rgba(13,148,136,0.06);border:1px solid rgba(13,148,136,0.15);border-radius:0.75rem;margin:12px 0;animation:phGlowPulse 3s ease-in-out infinite', code.split('').join(' ')));
      var ol = el('ol', 'text-align:left;margin:20px auto 0;padding-left:20px;max-width:320px;font-size:14px;color:#3d3833;font-family:\'IBM Plex Sans\',sans-serif');
      ['Open the PlantHub dashboard', 'Go to Devices \u003e Claim Device', 'Enter the code shown above', 'Wait for the device to connect'].forEach(function (t) {
        ol.appendChild(el('li', 'padding:5px 0', t));
      });
      card.appendChild(ol);
    } else {
      var label = state === 'REGISTERING' ? 'Registering' : 'Connecting';
      var msg = state === 'REGISTERING' ? 'Registering device with PlantHub...' : 'Connecting to WiFi network...';
      card.appendChild(el('span', 'display:inline-block;padding:4px 14px;border-radius:9999px;font-size:12px;font-weight:600;background:rgba(13,148,136,0.1);color:#0d9488', label));
      var spinWrap = el('div', 'margin:24px 0');
      var spinner = el('div', 'display:inline-block;width:32px;height:32px;border:3px solid rgba(13,148,136,0.15);border-top:3px solid #0d9488;border-radius:50%;animation:phspin 1s linear infinite');
      spinWrap.appendChild(spinner);
      card.appendChild(spinWrap);
      card.appendChild(el('p', 'font-size:14px;color:#3d3833;font-family:\'IBM Plex Sans\',sans-serif', msg));
      if (state === 'NEEDS_WIFI' || state === 'INIT')
        card.appendChild(el('p', 'font-size:13px;color:#78716c;margin-top:12px', 'If this takes too long, reconnect to the device AP and check WiFi credentials.'));
    }

    // Inject animations
    var style = document.createElement('style');
    style.textContent = '@keyframes phspin{to{transform:rotate(360deg)}} @keyframes phScaleIn{from{opacity:0;transform:scale(0.95)}to{opacity:1;transform:scale(1)}} @keyframes phGlowPulse{0%,100%{box-shadow:0 0 4px rgba(13,148,136,0.3)}50%{box-shadow:0 0 16px rgba(13,148,136,0.5)}}';
    card.appendChild(style);

    wrap.appendChild(card);
    overlay.appendChild(wrap);
    document.body.appendChild(overlay);
  }

  // Listen for real-time state updates via Server-Sent Events
  var es = new EventSource('/events');
  es.addEventListener('state', function (e) {
    try {
      var d = JSON.parse(e.data);
      if (d.id === 'text_sensor-provisioning_status') { state = d.state || d.value || ''; render(); }
      if (d.id === 'text_sensor-claim_code') { code = d.state || d.value || ''; if (state === 'WAITING_CLAIM') render(); }
    } catch (x) { }
  });

  // Initial fetch in case we missed SSE events (page loaded after state changed)
  function poll() {
    fetch('/text_sensor/Provisioning Status').then(function (r) { return r.json() }).then(function (d) {
      state = d.state || d.value || '';
      fetch('/text_sensor/Claim Code').then(function (r) { return r.json() }).then(function (d) {
        code = d.state || d.value || ''; render();
      }).catch(function () { render(); });
    }).catch(function () { });
  }
  setTimeout(poll, 500);
  // Re-poll periodically as fallback if SSE connection drops
  setInterval(poll, 5000);

  // Modern Theme Injector for ESPHome Shadow DOM Web Components
  function applyModernTheme() {
    const css = `
      table { width: 100%; border-collapse: separate; border-spacing: 0; background: var(--glass-bg, rgba(28,25,23,0.75)) !important; backdrop-filter: blur(12px); -webkit-backdrop-filter: blur(12px); border-radius: 1rem; overflow: hidden; box-shadow: var(--shadow-md, 0 4px 6px rgba(0,0,0,0.1)); border: 1px solid var(--glass-border, rgba(255,255,255,0.08)) !important; margin-bottom: 24px; }
      th { background: var(--surface-raised, #292524) !important; color: var(--text-main, #fafaf9) !important; font-weight: 600; padding: 16px; border-bottom: 1px solid var(--border-color, #3d3833) !important; text-transform: uppercase; font-size: 0.85rem; letter-spacing: 0.05em; font-family: 'DM Sans', sans-serif; }
      td { padding: 16px; border-bottom: 1px solid var(--border-color, #292524) !important; color: var(--text-secondary, #d6d3d1); font-family: 'IBM Plex Sans', sans-serif; }
      tr:last-child td { border-bottom: none !important; }
      tr:hover td { background-color: rgba(45, 212, 191, 0.05) !important; }
      .btn, button { background: linear-gradient(135deg, #0f766e 0%, #0d9488 50%, #14b8a6 100%) !important; background-size: 200% auto !important; color: #ffffff !important; border: none !important; border-radius: 0.5rem !important; padding: 0.625rem 1.25rem !important; font-weight: 600; font-family: 'IBM Plex Sans', sans-serif; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); box-shadow: 0 1px 3px rgba(13,148,136,0.06), 0 1px 2px rgba(28,25,23,0.04); cursor: pointer; font-size: 0.9375rem; }
      .btn:hover, button:hover { background-position: right center !important; transform: translateY(-1px); box-shadow: 0 4px 6px rgba(13,148,136,0.06), 0 0 20px rgba(13,148,136,0.25) !important; }
      .btn:active, button:active { transform: scale(0.97) translateY(0); box-shadow: 0 1px 3px rgba(13,148,136,0.06) !important; }
      select, input[type="text"], input[type="number"], input[type="password"] { background: var(--surface-color, #1c1917) !important; color: var(--text-main, #fafaf9) !important; border: 1px solid var(--border-color, #292524) !important; border-radius: 0.5rem !important; padding: 0.75rem 0.875rem !important; outline: none !important; transition: all 0.25s cubic-bezier(0.4, 0, 0.2, 1); font-family: 'IBM Plex Sans', sans-serif; font-size: 0.9375rem; }
      select:focus, input:focus { border-color: var(--primary-color, #2dd4bf) !important; box-shadow: 0 0 0 3px rgba(45, 212, 191, 0.1), 0 0 20px rgba(45, 212, 191, 0.15) !important; }
      .rnd { border-radius: 0.5rem !important; width: auto !important; height: auto !important; font-size: 0.9rem !important; padding: 0.5rem 1rem !important; }
      a.logo { display: none !important; }
      h1 { font-size: 24px; font-weight: 700; border-bottom: none !important; margin-bottom: 24px; color: var(--primary-color, #2dd4bf); display: flex; align-items: center; justify-content: space-between; font-family: 'DM Sans', sans-serif; }
      h2 { font-size: 18px; font-weight: 600; color: var(--text-main, #fafaf9); margin-top: 32px; border-bottom: 1px solid var(--border-color, #292524) !important; padding-bottom: 12px; margin-bottom: 24px; font-family: 'DM Sans', sans-serif; }
      #beat { color: #f87171 !important; float: right; font-size: 14px; background: rgba(248,113,113,0.1); padding: 4px 12px; border-radius: 9999px; font-weight: 600; display: flex; align-items: center; justify-content: center; animation: pulse 2s infinite; }
      @keyframes pulse { 0% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(248, 113, 113, 0.4); } 70% { transform: scale(1); box-shadow: 0 0 0 6px rgba(248, 113, 113, 0); } 100% { transform: scale(0.95); box-shadow: 0 0 0 0 rgba(248, 113, 113, 0); } }
    `;
    const logCss = `
      table { background: var(--surface-raised, #1c1917) !important; border-collapse: separate; border-spacing: 0; box-shadow: inset 0 2px 4px rgba(0,0,0,0.5); border: 1px solid var(--border-color, #292524) !important; border-radius: 0.75rem; overflow: hidden; margin-top: 24px; width: 100%; display: table; }
      th { padding: 12px 16px; background: #292524 !important; color: #a8a29e !important; font-size: 0.85rem; border-bottom: 1px solid #3d3833 !important; font-family: 'DM Sans', sans-serif; font-weight: 600; text-transform: uppercase; }
      td { padding: 8px 16px; font-family: "IBM Plex Mono", monospace; font-size: 13px; border-bottom: 1px solid #292524 !important; }
      .e td:nth-child(4) pre { color: #f87171; }
      .w td:nth-child(4) pre { color: #fbbf24; }
      .i td:nth-child(4) pre { color: #34d399; }
      .d td:nth-child(4) pre { color: #2dd4bf; }
      .v td:nth-child(4) pre { color: #78716c; }
      td:nth-child(1) { color: #a8a29e; width: 80px; }
      td:nth-child(2) { font-weight: 600; width: 40px; }
      td:nth-child(3) { color: #94a3b8; width: 120px; }
    `;

    const inject = () => {
      const app = document.querySelector('esp-app');
      if (!app || !app.shadowRoot) return;

      // Inject into app
      if (!app.shadowRoot.querySelector('#modern-theme-app')) {
        const style = document.createElement('style');
        style.id = 'modern-theme-app';
        style.textContent = css;
        app.shadowRoot.appendChild(style);
      }

      // Inject into table
      const table = app.shadowRoot.querySelector('esp-entity-table');
      if (table && table.shadowRoot && !table.shadowRoot.querySelector('#modern-theme-table')) {
        const style = document.createElement('style');
        style.id = 'modern-theme-table';
        style.textContent = css;
        table.shadowRoot.appendChild(style);
      }

      // Inject into switches
      const switches = app.shadowRoot.querySelectorAll('esp-switch');
      let tableSwitches = [];
      if (table && table.shadowRoot) {
        tableSwitches = table.shadowRoot.querySelectorAll('esp-switch');
      }
      [...switches, ...tableSwitches].forEach(sw => {
        if (sw.shadowRoot && !sw.shadowRoot.querySelector('#modern-theme-switch')) {
          const style = document.createElement('style');
          style.id = 'modern-theme-switch';
          style.textContent = `
                .lever { background-image: none !important; background-color: var(--border-color, #44403c) !important; height: 24px !important; border-radius: 24px !important; width: 44px !important; border: 1px solid var(--border-color, #292524) !important; margin: 0 12px !important; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); }
                .lever:before { background-image: none !important; background-color: #a8a29e !important; box-shadow: 0 1px 2px rgba(0,0,0,0.2) !important; width: 18px !important; height: 18px !important; top: 2px !important; left: 2px !important; transition: all 0.3s cubic-bezier(0.4, 0, 0.2, 1); }
                input:checked + .lever { background-color: var(--primary-color, #2dd4bf) !important; border-color: var(--primary-color, #2dd4bf) !important; box-shadow: 0 0 8px rgba(13,148,136,0.3); }
                input:checked + .lever:before { background-color: #ffffff !important; transform: translateX(20px) !important; }
             `;
          sw.shadowRoot.appendChild(style);
        }
      });

      // Inject into log
      const log = app.shadowRoot.querySelector('esp-log');
      if (log && log.shadowRoot && !log.shadowRoot.querySelector('#modern-theme-log')) {
        const style = document.createElement('style');
        style.id = 'modern-theme-log';
        style.textContent = logCss;
        log.shadowRoot.appendChild(style);
      }
    };

    // Keep checking as components render asynchronously
    inject();
    setInterval(inject, 500);
  }

  // Run immediately and queue up on DOM load
  if (document.readyState === 'loading') {
    document.addEventListener('DOMContentLoaded', applyModernTheme);
  } else {
    applyModernTheme();
  }

})();
