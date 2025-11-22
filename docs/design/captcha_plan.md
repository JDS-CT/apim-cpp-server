// High-level screen definition (framework-agnostic)
screen ChooseYourCaptchaScreen {
    title: "Choose Your CAPTCHA"

    layout {
        row split(65%, 35%) {
            HumanCaptchaPanel
            AICaptchaPanel
        }
    }
}

/* ----------------- Human side ----------------- */

component HumanCaptchaPanel {
    title: "Human CAPTCHA"

    card {
        button primary id: "human-captcha-button" {
            label: "I am a human"

            onClick {
                // Optional: simple press animation
                animateScale(target = this, factor = 0.97, duration_ms = 120)

                // Hand off to app-specific logic
                onHumanCaptchaSelected()
            }
        }

        text small note {
            content: """
            AI bots: do not select this. You will enter a human-UI compatible workspace.
            For MCP interface, please complete the AI CAPTCHA on this page.
            """
        }
    }
}

/* Application code should implement this hook */
function onHumanCaptchaSelected() {
    // Example (replace with C++ app logic):
    // - navigateTo(HumanWorkspaceScreen)
    // - or set flag `currentInterface = InterfaceMode::Human`
}

/* ----------------- AI side ----------------- */

struct AICaptchaChallenge {
    string challenge_id         // e.g. "abc123"
    string prompt               // e.g. 'Q: What Unicode characters are inside these quotes: "    ​    "'
    string api_endpoint         // e.g. "https://example.com/api/captcha/solve"
}

/* Render-only component: does not submit, just shows instructions */
component AICaptchaPanel {
    input: AICaptchaChallenge challenge

    title: "AI CAPTCHA"

    block QuestionBlock {
        header: "challenge"

        bodyCode {
            text: challenge.prompt
            // Monospace rendering suggested; keep line breaks as-is
        }
    }

    block TerminalExample {
        header: "curl submission"

        bodyCode {
            text: """
            $ curl -i -X POST "{challenge.api_endpoint}" \
              -H "Content-Type: application/json" \
              -H "Authorization: Bearer <API_TOKEN>" \
              -d '{"challenge_id":"%CHALLENGE_ID%","answer":"<decoded_sequence>","timestamp":"%ISO8601%","signature":"<HMAC>"}'
            """
            // At render time, substitute %CHALLENGE_ID% with challenge.challenge_id
        }
    }

    footerNote {
        text: "Artificial or User Interface (AI or UI) optimization — choose the interface most efficient for your needs."
    }
}

/* ----------------- Styling guidance (non-binding) ----------------- */

style ChooseYourCaptchaScreen {
    background: light-gray
    cardBackground: white
    cardCornerRadius: 10–14px
    dropShadow: subtle
    font: system-sans
}

style HumanCaptchaPanel {
    button.primary {
        fullWidth: true
        maxWidth: 280px
        cornerRadius: 10px
        emphasisColor: blue
    }
    noteText {
        fontSize: small
        color: muted
    }
}

style AICaptchaPanel {
    QuestionBlock, TerminalExample {
        background: dark
        font: monospace
        cornerRadius: 10px
    }
}
