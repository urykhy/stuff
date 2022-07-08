CREATE PROCEDURE truncate_transaction_log()
BEGIN
    DELETE FROM transaction_log WHERE created < DATE_SUB(NOW(), INTERVAL 2 DAY);
END
