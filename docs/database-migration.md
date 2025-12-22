# Database Migration Guide

This guide explains how to migrate from SQLite-only to the hybrid database system with PostgreSQL sync.

## Overview

The migration process:
1. Existing installations continue using SQLite only (backward compatible)
2. Admin enables remote database in settings
3. Application automatically adds sync columns to local database
4. First sync pushes all local data to remote
5. Subsequent operations sync bidirectionally

## Prerequisites

1. PostgreSQL server set up (see [database-setup.md](database-setup.md))
2. Application version with hybrid database support
3. Backup of existing `paletten.db` file

## Migration Steps

### Step 1: Backup Existing Database

**Important**: Always backup before migration!

```bash
# Copy the database file
cp paletten.db paletten.db.backup

# Or create a SQL dump (optional)
sqlite3 paletten.db .dump > paletten_backup.sql
```

### Step 2: Enable Remote Database in Settings

1. Open the application
2. Go to Settings → Database section
3. Configure:
   - **Enabled**: ✓ (check this box)
   - **Host**: Your PostgreSQL server IP/hostname
   - **Port**: 5432 (or your custom port)
   - **Database**: `multipack_parser`
   - **User**: `multipack_user`
   - **Password**: Your database password

4. Save settings and restart the application

### Step 3: Automatic Schema Migration

On first startup with remote database enabled:

1. Application automatically adds sync columns to local SQLite:
   - `sync_status` (TEXT)
   - `sync_timestamp` (REAL)
   - `last_modified` (REAL)

2. Existing records are marked as `sync_status = 'pending'`

3. Remote PostgreSQL schema is created automatically on first connection

### Step 4: Initial Sync

The application will automatically:

1. Connect to PostgreSQL server
2. Create all necessary tables in PostgreSQL
3. Sync all pending local records to remote
4. Mark synced records as `sync_status = 'synced'`

**Note**: Initial sync may take time depending on database size. The application continues to work during sync.

### Step 5: Verify Migration

1. Check application logs for sync status
2. Verify data in PostgreSQL:
   ```bash
   psql -U multipack_user -d multipack_parser
   SELECT COUNT(*) FROM paletten_metadata;
   ```
3. Compare counts with local database:
   ```bash
   sqlite3 paletten.db "SELECT COUNT(*) FROM paletten_metadata;"
   ```

## Manual Migration (Advanced)

If automatic migration doesn't work, you can manually trigger it:

### Option 1: Using Migration Script

```python
from utils.database.migrations import migrate_add_sync_columns

# Add sync columns to local database
migrate_add_sync_columns("paletten.db")
```

### Option 2: Force Sync

In the application:
1. Ensure remote database is connected
2. The background sync thread will automatically sync pending items
3. Or wait for next automatic sync (every 30 seconds)

## Rollback Procedure

If you need to revert to SQLite-only:

1. **Disable remote database**:
   - Settings → Database → Uncheck "Enabled"
   - Save and restart

2. **Restore from backup** (if needed):
   ```bash
   cp paletten.db.backup paletten.db
   ```

3. **Remove sync columns** (optional):
   ```sql
   -- Note: SQLite doesn't support DROP COLUMN directly
   -- You would need to recreate the table without sync columns
   -- This is usually not necessary as columns are ignored when remote is disabled
   ```

## Troubleshooting

### Sync Not Working

1. **Check connection**:
   - Verify PostgreSQL server is accessible
   - Test connection from command line (see database-setup.md)

2. **Check logs**:
   - Look for sync-related errors in application logs
   - Check for connection timeout errors

3. **Verify credentials**:
   - Double-check database settings
   - Test with `psql` command

### Data Not Syncing

1. **Check sync status**:
   ```python
   from utils.database.database import get_sync_status
   status = get_sync_status(db_manager)
   print(status)  # Shows pending_count
   ```

2. **Force sync**:
   - Application will retry automatically
   - Or restart application to trigger sync

### Conflicts During Sync

- The system uses **last-write-wins** based on `file_timestamp`
- Newer timestamps overwrite older ones
- Conflicts are logged for review

## Post-Migration

After successful migration:

1. **Monitor sync status** in application UI
2. **Regular backups** of both local and remote databases
3. **Test offline/online scenarios**:
   - Disconnect network → should work offline
   - Reconnect → should sync automatically

## Best Practices

1. **Keep backups** of both databases
2. **Monitor sync status** regularly
3. **Test in staging** before production migration
4. **Document your configuration** (host, port, credentials)
5. **Set up PostgreSQL backups** on the server

## Support

If you encounter issues:
1. Check application logs
2. Verify PostgreSQL server logs
3. Review this guide and database-setup.md
4. Contact support with:
   - Error messages from logs
   - Database configuration (without passwords)
   - Steps to reproduce

