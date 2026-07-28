/**
 * Send transactional email via Resend, or log the link in development.
 */

export async function sendVerifyEmail(to: string, verifyUrl: string): Promise<void> {
  const apiKey = process.env.RESEND_API_KEY;
  const from = process.env.EMAIL_FROM ?? "Sticky Notes <onboarding@resend.dev>";

  if (!apiKey) {
    console.warn(`[email] RESEND_API_KEY unset — verify link for ${to}:`);
    console.warn(`  ${verifyUrl}`);
    return;
  }

  const res = await fetch("https://api.resend.com/emails", {
    method: "POST",
    headers: {
      Authorization: `Bearer ${apiKey}`,
      "Content-Type": "application/json",
    },
    body: JSON.stringify({
      from,
      to: [to],
      subject: "Verify your Sticky Notes account",
      html: `<p>Welcome to Sticky Notes.</p>
<p><a href="${verifyUrl}">Click here to verify your email</a>.</p>
<p>Or paste this URL: ${verifyUrl}</p>
<p>This link expires in 24 hours.</p>`,
    }),
  });

  if (!res.ok) {
    const text = await res.text();
    throw new Error(`Resend failed: ${res.status} ${text}`);
  }
}
