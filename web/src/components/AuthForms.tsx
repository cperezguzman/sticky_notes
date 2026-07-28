interface AuthFormsProps {
  mode: "login" | "signup" | "check_email";
  storage: "cloud" | "files" | null;
  busy: boolean;
  error: string | null;
  info: string | null;
  checkEmailAddress: string | null;
  onLogin: (email: string, password: string) => void;
  onSignup: (email: string, password: string) => void;
  onResend: (email: string) => void;
  onShowLogin: () => void;
  onShowSignup: () => void;
}

export function AuthForms({
  mode,
  storage,
  busy,
  error,
  info,
  checkEmailAddress,
  onLogin,
  onSignup,
  onResend,
  onShowLogin,
  onShowSignup,
}: AuthFormsProps) {
  const cloud = storage !== "files";

  if (mode === "check_email") {
    return (
      <div className="login-shell">
        <div className="login-card">
          <h1>Check your email</h1>
          <p className="login-lead">
            We sent a verification link
            {checkEmailAddress ? (
              <>
                {" "}
                to <strong>{checkEmailAddress}</strong>
              </>
            ) : null}
            . Open it, then sign in.
          </p>
          <p className="login-hint">
            Locally, the link is printed in the API server terminal (no Resend
            key).
          </p>
          {info ? <p className="login-info">{info}</p> : null}
          {error ? <p className="login-error">{error}</p> : null}
          {checkEmailAddress ? (
            <button
              type="button"
              className="btn"
              disabled={busy}
              onClick={() => onResend(checkEmailAddress)}
            >
              {busy ? "Sending…" : "Resend verification link"}
            </button>
          ) : null}
          <button type="button" className="btn primary" onClick={onShowLogin}>
            Back to sign in
          </button>
        </div>
      </div>
    );
  }

  return (
    <div className="login-shell">
      <form
        className="login-card"
        onSubmit={(e) => {
          e.preventDefault();
          const fd = new FormData(e.currentTarget);
          const email = String(fd.get("email") ?? "");
          const password = String(fd.get("password") ?? "");
          if (mode === "signup") {
            onSignup(email, password);
          } else {
            onLogin(email, password);
          }
        }}
      >
        <h1>Sticky Notes</h1>
        <p className="login-lead">
          {cloud
            ? "Notes sync to your account in the cloud."
            : "Local file mode — sign in to edit notes on disk."}
        </p>

        <div className="auth-tabs">
          <button
            type="button"
            className={mode === "login" ? "tab active" : "tab"}
            onClick={onShowLogin}
            disabled={busy}
          >
            Sign in
          </button>
          {cloud ? (
            <button
              type="button"
              className={mode === "signup" ? "tab active" : "tab"}
              onClick={onShowSignup}
              disabled={busy}
            >
              Create account
            </button>
          ) : null}
        </div>

        <label className="field">
          <span>{cloud ? "Email" : "Username or email"}</span>
          <input
            name="email"
            type={cloud ? "email" : "text"}
            autoComplete={cloud ? "email" : "username"}
            defaultValue={cloud ? "" : "admin"}
            disabled={busy}
            required
          />
        </label>
        <label className="field">
          <span>Password</span>
          <input
            name="password"
            type="password"
            autoComplete={
              mode === "signup" ? "new-password" : "current-password"
            }
            disabled={busy}
            required={mode === "signup" || mode === "login"}
            minLength={cloud && mode === "signup" ? 8 : undefined}
          />
        </label>
        {error ? <p className="login-error">{error}</p> : null}
        {info ? <p className="login-info">{info}</p> : null}
        <button type="submit" className="btn primary" disabled={busy}>
          {busy
            ? "Please wait…"
            : mode === "signup"
              ? "Create account"
              : "Sign in"}
        </button>
        {cloud && mode === "login" ? (
          <button
            type="button"
            className="btn linkish"
            disabled={busy}
            onClick={(e) => {
              const form = e.currentTarget.form;
              const email = form
                ? String(new FormData(form).get("email") ?? "")
                : "";
              onResend(email);
            }}
          >
            Resend verification email
          </button>
        ) : null}
        {!cloud ? (
          <p className="login-hint">
            File-mode default: <code>admin</code> / <code>sticky-notes</code>
          </p>
        ) : (
          <p className="login-hint">
            After signup, verify your email before signing in. If the link
            expired, use Resend verification above (or Create account again).
          </p>
        )}
      </form>
    </div>
  );
}
