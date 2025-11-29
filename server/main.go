package main

import (
	"log"
	. "messangere/database"
	"net/http"
	"os"
	"path/filepath"
	"strconv"

	"github.com/gin-gonic/gin"
	"github.com/google/uuid"
	"gorm.io/gorm"
)

type Repository struct {
	DB *gorm.DB
}

const storageDir = "./storage"

func (r *Repository) uploadHandler(c *gin.Context) {
	form, err := c.MultipartForm()
	if err != nil {
		c.JSON(http.StatusBadRequest, gin.H{
			"message": "file not found",
		})
		return
	}
	files := form.File["file"]
	if len(files) == 0 {
		c.JSON(http.StatusBadRequest, gin.H{
			"message": "no files received",
		})
		return
	}
	var successuploads []Files
	for _, file := range files {
		tmpfilename := uuid.New().String() + filepath.Ext(file.Filename)
		temppath := filepath.Join(storageDir, tmpfilename)

		if err := c.SaveUploadedFile(file, temppath); err != nil {
			c.JSON(http.StatusInternalServerError, gin.H{
				"message": "can't save temporary file",
			})
			return
		}

		filerecord := Files{
			Name:     file.Filename,
			Mimetype: file.Header.Get("Content-Type"),
			Size:     uint64(file.Size),
		}
		err = r.DB.Create(&filerecord).Error
		if err != nil {
			os.Remove(temppath)
			c.JSON(http.StatusInternalServerError, gin.H{
				"message": "couldn't create record in DB",
			})
			log.Printf("Failed to create DB record for %s: %v", file.Filename, err)
			continue
		}
		finalfilename := strconv.FormatUint(filerecord.ID, 10) + filepath.Ext(file.Filename)
		finalpath := filepath.Join(storageDir, finalfilename)

		if err := os.Rename(temppath, finalpath); err != nil {
			os.Remove(temppath)
			r.DB.Delete(&filerecord)
			c.JSON(http.StatusInternalServerError, gin.H{
				"message": "can't rename the file",
			})
			log.Printf("Failed to rename file %s: %v", file.Filename, err)
			continue
		}

		filerecord.StoragePath = finalpath
		r.DB.Save(&filerecord)
		successuploads = append(successuploads, filerecord)
	}
	if len(successuploads) == 0 {
		c.JSON(http.StatusInternalServerError, gin.H{
			"message": "no one files could be uploaded",
		})
		return
	}

	c.JSON(http.StatusOK, gin.H{
		"message": "files uploaded successfully",
		"data":    successuploads,
	})
}
func (r *Repository) downloadHandler(c *gin.Context) {
	param := c.Param("id")

	filerecord := Files{}

	err := r.DB.Where("id=?", param).First(&filerecord).Error
	if err != nil {
		c.JSON(http.StatusNotFound, gin.H{
			"message": "can't found",
		})
		return
	}
	c.FileAttachment(filerecord.StoragePath, filerecord.Name)

}

func main() {
	if err := os.MkdirAll(storageDir, 0755); err != nil {
		log.Fatal("coudn't create the directory")
	}
	router := gin.Default()
	db, err := Connection()

	if err != nil {
		log.Fatal("could not load the database")
	}
	err = MigrateDB(db)
	if err != nil {
		log.Fatal("could not migrate db")
	}
	r := Repository{
		DB: db,
	}
	api := router.Group("/files")
	{
		api.GET("/download/:id", r.downloadHandler)
		api.POST("/upload", r.uploadHandler)
		// api.Get("/")
	}

	router.Run(":9090")
}
