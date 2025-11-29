package database

import (
	"gorm.io/driver/postgres"
	"gorm.io/gorm"
)

type Files struct {
	ID          uint64 `gorm:"primary key;autoIncrement" json:"id"`
	Name        string `json:"name"`
	Mimetype    string `json:"mimetype"`
	StoragePath string `json:"storage_path"`
	Size        uint64 `json:"size"`
}

func MigrateDB(db *gorm.DB) error {
	err := db.AutoMigrate(&Files{})
	return err
}
func Connection() (*gorm.DB, error) {
	dsn := "host=localhost user=postgres password=123 dbname=messenger_files port=12345 sslmode=disable"
	db, err := gorm.Open(postgres.Open(dsn), &gorm.Config{})
	if err != nil {
		return db, err
	}
	return db, nil
}
