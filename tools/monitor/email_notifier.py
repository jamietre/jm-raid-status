#!/usr/bin/env python3
"""
Email notification module for RAID monitor.
Handles loading config and sending email alerts.
"""

import smtplib
from email.mime.text import MIMEText
from email.mime.multipart import MIMEMultipart
from pathlib import Path


def load_email_config(env_file):
    """Load email configuration from .env file."""
    config = {}
    env_path = Path(env_file)

    if not env_path.exists():
        return None, f"Config file not found: {env_file}"

    try:
        with open(env_path, "r") as f:
            for line in f:
                line = line.strip()
                if not line or line.startswith("#"):
                    continue
                if ":" in line:
                    key, value = line.split(":", 1)
                    config[key.strip()] = value.strip()

        required = ["smtp_server", "authuser", "authpass"]
        missing = [k for k in required if k not in config]
        if missing:
            return None, f"Missing required config: {', '.join(missing)}"

        return config, None
    except Exception as e:
        return None, f"Error reading config: {e}"


def send_email(config, subject, body, to_address=None):
    """
    Send email notification.

    Args:
        config: Email configuration dict with smtp_server, authuser, authpass, ecrypttion
        subject: Email subject line
        body: Email body text
        to_address: Recipient address (defaults to authuser if None)

    Returns:
        (success: bool, error_message: str or None)
    """
    try:
        # Use sender as recipient if not specified
        recipient = to_address or config["authuser"]

        # Create message
        msg = MIMEMultipart()
        msg["From"] = config["authuser"]
        msg["To"] = recipient
        msg["Subject"] = subject
        msg.attach(MIMEText(body, "plain"))

        # Parse SMTP server and port
        smtp_parts = config["smtp_server"].split(":")
        smtp_server = smtp_parts[0]
        smtp_port = int(smtp_parts[1]) if len(smtp_parts) > 1 else 465

        # Connect and send
        if config.get("ecrypttion") == "ssl":
            server = smtplib.SMTP_SSL(smtp_server, smtp_port, timeout=30)
        else:
            server = smtplib.SMTP(smtp_server, smtp_port, timeout=30)
            server.starttls()

        server.login(config["authuser"], config["authpass"])
        server.send_message(msg)
        server.quit()

        return True, None
    except smtplib.SMTPAuthenticationError as e:
        return False, f"Authentication failed: {e}"
    except smtplib.SMTPException as e:
        return False, f"SMTP error: {e}"
    except Exception as e:
        return False, f"Unexpected error: {e}"


if __name__ == "__main__":
    # Simple test when run directly
    import sys

    if len(sys.argv) < 2:
        print("Usage: python3 email_notifier.py <path-to-.env> [test-message]")
        print("Example: python3 email_notifier.py ../../.env")
        sys.exit(1)

    env_file = sys.argv[1]
    test_message = sys.argv[2] if len(sys.argv) > 2 else "This is a test email from the RAID monitor."

    print(f"Loading email config from: {env_file}")
    config, error = load_email_config(env_file)

    if error:
        print(f"ERROR: {error}")
        sys.exit(1)

    print(f"Config loaded successfully:")
    print(f"  SMTP Server: {config['smtp_server']}")
    print(f"  Auth User: {config['authuser']}")
    print(f"  Encryption: {config.get('ecrypttion', 'none')}")
    print()

    subject = "RAID Monitor Test Email"
    body = f"""{test_message}

If you received this email, the RAID monitoring email system is working correctly.

Test details:
- SMTP Server: {config['smtp_server']}
- From: {config['authuser']}
- To: {config['authuser']}
"""

    print("Sending test email...")
    success, error = send_email(config, subject, body)

    if success:
        print("✓ Email sent successfully!")
        print(f"  Check inbox for: {config['authuser']}")
    else:
        print(f"✗ Failed to send email: {error}")
        sys.exit(1)
