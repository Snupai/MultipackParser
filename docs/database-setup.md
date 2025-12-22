# Database Server Setup Guide

This guide explains how to set up a PostgreSQL database server for the MultipackParser application's shared database functionality.

## Overview

The application supports a hybrid database architecture:
- **Local SQLite**: Always available, works offline
- **Remote PostgreSQL**: Shared database for multiple instances when online

## PostgreSQL Installation

### Windows

1. Download PostgreSQL from [postgresql.org/download/windows](https://www.postgresql.org/download/windows/)
2. Run the installer
3. During installation:
   - Set a password for the `postgres` superuser
   - Note the port (default: 5432)
   - Choose installation directory

### Linux (Ubuntu/Debian)

```bash
sudo apt-get update
sudo apt-get install postgresql postgresql-contrib
sudo systemctl start postgresql
sudo systemctl enable postgresql
```

### Linux (CentOS/RHEL)

```bash
sudo yum install postgresql-server postgresql-contrib
sudo postgresql-setup initdb
sudo systemctl start postgresql
sudo systemctl enable postgresql
```

### Docker

Quick setup using Docker:

```bash
docker run -d \
  --name multipack-db \
  -e POSTGRES_PASSWORD=yourpassword \
  -e POSTGRES_DB=multipack_parser \
  -p 5432:5432 \
  postgres:15
```

## Database Creation

### Using psql Command Line

1. Connect to PostgreSQL as superuser:
   ```bash
   sudo -u postgres psql
   # Or on Windows: psql -U postgres
   ```

2. Create database and user:
   ```sql
   CREATE DATABASE multipack_parser;
   CREATE USER multipack_user WITH PASSWORD 'your_secure_password';
   GRANT ALL PRIVILEGES ON DATABASE multipack_parser TO multipack_user;
   \q
   ```

3. Connect to the new database and grant schema privileges:
   ```bash
   psql -U multipack_user -d multipack_parser
   ```
   ```sql
   GRANT ALL ON SCHEMA public TO multipack_user;
   \q
   ```

### Using pgAdmin (GUI)

1. Open pgAdmin
2. Connect to PostgreSQL server
3. Right-click "Databases" → "Create" → "Database"
   - Name: `multipack_parser`
4. Expand "multipack_parser" → "Schemas" → "public"
5. Right-click "Login/Group Roles" → "Create" → "Login/Group Role"
   - Name: `multipack_user`
   - Password: Set a secure password
   - Privileges: Grant all necessary permissions

## Network Configuration

### Configure PostgreSQL to Accept Connections

1. Edit `postgresql.conf`:
   ```bash
   # Location depends on OS:
   # Linux: /etc/postgresql/<version>/main/postgresql.conf
   # Windows: C:\Program Files\PostgreSQL\<version>\data\postgresql.conf
   ```

2. Set `listen_addresses`:
   ```
   listen_addresses = '*'  # Or specific IP addresses
   ```

3. Edit `pg_hba.conf`:
   ```bash
   # Location: same directory as postgresql.conf
   ```

4. Add connection rules:
   ```
   # Allow connections from local network
   host    multipack_parser    multipack_user    192.168.0.0/24    md5
   # Or allow from any IP (less secure):
   host    multipack_parser    multipack_user    0.0.0.0/0         md5
   ```

5. Restart PostgreSQL:
   ```bash
   # Linux
   sudo systemctl restart postgresql
   
   # Windows: Use Services manager or:
   # Restart-Service postgresql-x64-<version>
   ```

### Firewall Configuration

**Linux (ufw):**
```bash
sudo ufw allow 5432/tcp
```

**Linux (firewalld):**
```bash
sudo firewall-cmd --permanent --add-service=postgresql
sudo firewall-cmd --reload
```

**Windows:**
1. Open Windows Firewall
2. Add inbound rule for port 5432 (TCP)

## Connection Testing

### Test from Command Line

```bash
psql -h <server_ip> -U multipack_user -d multipack_parser
# Enter password when prompted
```

### Test from Python

```python
import psycopg2

try:
    conn = psycopg2.connect(
        host="your_server_ip",
        port=5432,
        database="multipack_parser",
        user="multipack_user",
        password="your_password"
    )
    print("Connection successful!")
    conn.close()
except Exception as e:
    print(f"Connection failed: {e}")
```

## Application Configuration

1. Open the application settings
2. Navigate to Database section
3. Configure:
   - **Enabled**: Check to enable remote database
   - **Host**: IP address or hostname of PostgreSQL server
   - **Port**: 5432 (default)
   - **Database**: `multipack_parser`
   - **User**: `multipack_user`
   - **Password**: Your configured password

4. Restart the application

## Security Best Practices

1. **Use strong passwords** for database users
2. **Limit network access** in `pg_hba.conf` to specific IP ranges
3. **Use SSL/TLS** for remote connections (configure in `postgresql.conf`)
4. **Regular backups** of the database
5. **Keep PostgreSQL updated** with security patches

## Troubleshooting

### Connection Refused

- Check PostgreSQL is running: `sudo systemctl status postgresql`
- Verify `listen_addresses` in `postgresql.conf`
- Check firewall rules
- Verify `pg_hba.conf` allows your IP

### Authentication Failed

- Verify username and password
- Check `pg_hba.conf` authentication method (md5, scram-sha-256)
- Ensure user has proper permissions

### Permission Denied

- Grant privileges: `GRANT ALL ON DATABASE multipack_parser TO multipack_user;`
- Grant schema privileges: `GRANT ALL ON SCHEMA public TO multipack_user;`

## Backup and Restore

### Backup

```bash
pg_dump -U multipack_user -d multipack_parser -F c -f backup.dump
```

### Restore

```bash
pg_restore -U multipack_user -d multipack_parser backup.dump
```

## Next Steps

After setting up the database server:
1. Configure the application to connect (see Application Configuration)
2. The application will automatically create the schema on first connection
3. Existing local data will sync to the remote database automatically

